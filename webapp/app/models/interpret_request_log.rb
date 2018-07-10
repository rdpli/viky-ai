class InterpretRequestLog

  INDEX_NAME = ['stats-interpret_request_log', Rails.env].join('-').freeze
  INDEX_ALIAS_NAME = ['index', INDEX_NAME].join('-').freeze
  SEARCH_ALIAS_NAME = ['search', INDEX_NAME].join('-').freeze

  INDEX_TYPE = 'log'.freeze

  attr_reader :id, :timestamp, :sentence, :language, :now

  def self.count(params = {})
    client = IndexManager.client
    result = client.count index: SEARCH_ALIAS_NAME, body: params
    result['count']
  end

  def self.find(id)
    client = IndexManager.client
    result = client.get index: SEARCH_ALIAS_NAME, type: INDEX_TYPE, id: id
    params = result['_source'].symbolize_keys
    params[:id] = result['_id']
    InterpretRequestLog.new params
  end

  def initialize(params = {})
    @id = params[:id]
    @agent = params[:agent].present? ? params[:agent] : Agent.find(params[:agent_id])
    @timestamp = params[:timestamp]
    @sentence = params.fetch(:sentence, '')
    @language = params[:language]
    @now = params[:now]
  end

  def save
    refresh = Rails.env == 'test'
    client = IndexManager.client
    result = client.index index: INDEX_ALIAS_NAME, type: INDEX_TYPE, body: to_json, id: @id, refresh: refresh
    @id = result['_id']
  end


  private

    def to_json
      {
        timestamp: @timestamp,
        sentence: @sentence,
        language: @language,
        now: @now,
        agent_id: @agent.id,
        owner_id: @agent.owner.id
      }
    end
end
