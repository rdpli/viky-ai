require_relative 'lib/task'

namespace :statistics do

  desc 'Creates and configures stats indices'
  task setup: :environment do |t, args|
    environments = [Rails.env]
    environments << 'test' if Rails.env == 'development'
    environments.each do |environment|
      Task::Print.step("Environment #{environment}.")
      # use a client with a bigger timeout
      client = IndexManager.client(
        transport_options: {
          request: { timeout: 5.minutes }
        }
      )
      unless cluster_ready?(client)
        Task::Print.error('Cannot perform tasks : cluster is not ready')
        exit 1
      end
      IndexManager.fetch_template_configurations.each do |template_conf|
        active_template, = save_template(client, template_conf)
        Task::Print.substep("Index #{active_template.index_name}.")

        alias_name = InterpretRequestLog::INDEX_ALIAS_NAME
        if index_exists?(client, alias_name)
          Task::Print.notice("Delete index with same name as alias (#{alias_name}) because it should not exists.")
          client.indices.delete index: alias_name
        end

        index_with_valid_regexp_pattern = "#{active_template.index_patterns[0..-2]}.*"
        if index_exists?(client, index_with_valid_regexp_pattern)
          Task::Print.notice("Index like #{active_template.index_patterns} already exists : skipping index creation.")
        else
          index = StatisticsIndex.from_template active_template
          create_index(client, index)
          update_index_aliases(client, [{ add: { index: index.name, alias: alias_name } }], alias_name)
        end

      end
    end
    unless Rails.env == 'test'
      Task::Print.step("Configure Kibana.")
      begin
        StatisticsVisualizer.new.configure
      rescue RuntimeError => e
        Task::Print.error(e)
      end
    end
  end


  desc 'Reindex the specified statistics index into a new one'
  task :reindex, [:src_index] => :environment do |t, args|
    Task::Print.error('Missing param: src index') unless args.src_index.present?
    src_name = args.src_index
    # use a client with a bigger timeout
    client = IndexManager.client(
      transport_options: {
        request: { timeout: 5.minutes }
      }
    )
    unless cluster_ready?(client)
      Task::Print.error('Cannot perform tasks : cluster is not ready')
      exit 1
    end
    unless index_exists?(client, src_name)
      Task::Print.error("Source index #{src_name} does not exists.")
      exit 1
    end
    template_conf = IndexManager.fetch_template_configurations('template-stats-interpret_request_log').first
    active_template, inactive_template = save_template(client, template_conf)
    src_index = StatisticsIndex.from_name src_name
    template = src_index.active? ? active_template : inactive_template
    if src_index.need_reindexing? template
      dest_index = StatisticsIndex.from_template template
      Task::Print.step("Reindex #{src_index.name} to #{dest_index.name}.")
      reindex_into_new(client, src_index, dest_index)
    else
      Task::Print.notice("No need to reindex #{src_index.name} : skipping.")
    end
  end


  namespace :reindex do
    desc 'Reindex all statistics indices'
    task :all => :environment do |t, args|
      # use a client with a bigger timeout
      client = IndexManager.client(
        transport_options: {
          request: { timeout: 5.minutes }
        }
      )
      unless cluster_ready?(client)
        Task::Print.error('Cannot perform tasks : cluster is not ready')
        exit 1
      end
      template_conf = IndexManager.fetch_template_configurations('template-stats-interpret_request_log').first
      active_template, inactive_template = save_template(client, template_conf)
      indices = client.indices.get(index: 'stats-*').keys
                              .map { |index_name| StatisticsIndex.from_name index_name }
                              .partition { |index| index.active? }
      active_indices = indices.first.select { |index| index.need_reindexing? active_template }
      inactive_indices = indices.second.select { |index| index.need_reindexing? inactive_template }
      if (active_indices + inactive_indices).empty?
        Task::Print.step('Nothing to reindex.')
      else
        Task::Print.step("Reindex indices #{(active_indices.map(&:name) + inactive_indices.map(&:name))}.")
        active_indices.each do |src_index|
          dest_index = StatisticsIndex.from_template active_template
          Task::Print.substep("Reindex #{src_index.name} to #{dest_index.name}.")
          reindex_into_new(client, src_index, dest_index)
        end
        inactive_indices.each do |src_index|
          dest_index = StatisticsIndex.from_template inactive_template
          Task::Print.substep("Reindex #{src_index.name} to #{dest_index.name}.")
          reindex_into_new(client, src_index, dest_index)
        end
      end
    end
  end


  desc 'Roll over index if older than 7 days or have more than 100 000 documents'
  task :rollover => :environment do |t, args|
    max_age = '7d'
    max_docs = 100_000
    Task::Print.step("Roll over alias #{InterpretRequestLog::INDEX_ALIAS_NAME} with conditions max_age=#{max_age} or max_docs=#{max_docs}.")
    # use a client with a bigger timeout
    client = IndexManager.client(
      transport_options: {
        request: { timeout: 5.minutes }
      }
    )
    unless cluster_ready?(client)
      Task::Print.error('Cannot perform tasks : cluster is not ready')
      exit 1
    end
    template_conf = IndexManager.fetch_template_configurations('template-stats-interpret_request_log').first
    active_template = StatisticsIndexTemplate.new template_conf
    dest_index = StatisticsIndex.from_template active_template
    res = client.indices.rollover(alias: InterpretRequestLog::INDEX_ALIAS_NAME, new_index: dest_index.name, body:{
      conditions: {
        max_age: max_age,
        max_docs: max_docs,
      }
    })
    unless res['rolled_over']
      Task::Print.notice('No need to roll over because no condition reached.')
      exit 0
    end
    old_index = res['old_index']
    Task::Print.substep("Index #{old_index} rolled over to #{dest_index.name}.")
    shrink_node_name = pick_random_node(client)
    client.indices.put_settings index: old_index, body: {
      'index.routing.allocation.require._name' => shrink_node_name,
      'index.blocks.write' => true
    }
    Task::Print.substep("Index #{old_index} switched to read only and migrating to #{shrink_node_name}.")
    client.cluster.health wait_for_no_relocating_shards: true
    Task::Print.substep('Shards migration completed.')
    inactive_template = StatisticsIndexTemplate.new template_conf, 'inactive'
    target_name = StatisticsIndex.from_template inactive_template
    client.indices.shrink index: old_index, target: target_name.name
    Task::Print.substep("Index #{old_index} shrink into #{target_name.name}.")
    client.indices.forcemerge index: target_name.name, max_num_segments: 1
    Task::Print.substep("Index #{target_name.name} force merged.")
    client.indices.put_settings index: target_name.name, body: {
      'index.number_of_replicas' => 1
    }
    Task::Print.substep("Configured a replica for index #{target_name.name}.")
    update_index_aliases(client, [
      { add: { index: target_name.name, alias: InterpretRequestLog::SEARCH_ALIAS_NAME } },
      { remove: { index: old_index, alias: InterpretRequestLog::SEARCH_ALIAS_NAME } }
    ],
      InterpretRequestLog::SEARCH_ALIAS_NAME)
    delete_index(client, old_index)
  end


  private
    def save_template(client, template_conf)
      active_template = StatisticsIndexTemplate.new template_conf, 'active'
      inactive_template = StatisticsIndexTemplate.new template_conf, 'inactive'
      [active_template, inactive_template]
        .select { |template| template_exists?(client, template) }
        .each do |template|
        client.indices.put_template name: template.name, body: template.configuration
        Task::Print.success("Save index template #{template.name} succeed.")
      end
      return active_template, inactive_template
    end

    def template_exists?(client, template)
      exists = client.indices.exists_template? name: template.name
      Task::Print.notice("Template #{template.name} already exists : skipping.") if exists
      exists
    end

    def reindex_into_new(client, src_index, dest_index)
      create_index(client, dest_index)
      if src_index.active?
        update_index_aliases(client, [
          { remove: { index: src_index.name, alias: InterpretRequestLog::INDEX_ALIAS_NAME } },
          { add: { index: dest_index.name, alias: InterpretRequestLog::INDEX_ALIAS_NAME } }
        ],
          InterpretRequestLog::INDEX_ALIAS_NAME)
      else
        client.indices.put_settings index: dest_index.name, body: {
          'index.number_of_replicas' => 1
        }
      end
      reindex(src_index, dest_index)
      begin
        update_index_aliases(client, [
            { remove: { index: src_index.name, alias: InterpretRequestLog::SEARCH_ALIAS_NAME } },
          ],
          InterpretRequestLog::SEARCH_ALIAS_NAME)
      rescue Elasticsearch::Transport::Transport::Errors::NotFound => e
        # alias does not exist
      end
      delete_index(client, src_index.name)
    end

    def index_exists?(client, index_to_find)
      expected_status = Rails.env.production? ? 'green' : 'yellow'
      client.cluster.health(level: 'indices', wait_for_status: expected_status)['indices'].keys.any? do |index|
        index =~ /^#{index_to_find}$/i
      end
    end

    def create_index(client, index)
      client.indices.create index: index.name
      Task::Print.success("Creation of index #{index.name} succeed.")
    end

    def update_index_aliases(client, actions, index_alias)
      client.indices.update_aliases body: { actions: actions }
      Task::Print.success("Update aliases #{index_alias} succeed.")
    end

    def reindex(src_index, dest_index)
      Task::Print.substep("Wait for reindexing of #{src_index.name} to #{dest_index.name} ...")
      # use a client with a bigger timeout
      client = IndexManager.client(
        transport_options: {
          request: { timeout: 5.minutes }
        }
      )
      result = client.reindex(
        wait_for_completion: true,
        body: {
          source: { index: src_index.name },
          dest: { index: dest_index.name }
        }
      )

      Task::Print.success("Reindexing of #{src_index.name} to #{dest_index.name} succeed.")
      Task::Print.notice("Reindexing result : #{result.to_json}")
    end

    def delete_index(client, index_name)
      client.indices.delete index: index_name
      Task::Print.success("Remove index #{index_name} succeed.")
    end

    def pick_random_node(client)
      node_stats = client.nodes.info()['nodes']
      shrink_node = node_stats.keys.sample
      node_stats[shrink_node]['name']
    end

    def cluster_ready?(client)
      expected_status = Rails.env.production? ? 'green' : 'yellow'
      retry_count = 0
      max_retries = 3
      cluster_ready = false
      timetout = '30s'
      while !cluster_ready && retry_count < max_retries do
        Task::Print.substep("Wait for #{timetout} statistics cluster to be ready...")
        begin
          client.cluster.health(wait_for_status: expected_status, timeout: timetout)
          cluster_ready = true
        rescue Elasticsearch::Transport::Transport::Errors::RequestTimeout => e
          retry_count += 1
          Task::Print.notice("Cluster is not ready for the #{retry_count}/#{max_retries} attempt")
        end
      end
      cluster_ready
    end
end
