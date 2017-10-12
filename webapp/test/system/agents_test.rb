require "application_system_test_case"

class AgentsTest < ApplicationSystemTestCase

  test 'Navigation to agents index' do
    go_to_agents_index
    assert page.has_text?("Agents management")
    assert_equal "My awesome weather bot", first('.agent-box h2').text
  end


  test 'blank slate' do
    Agent.delete_all
    go_to_agents_index
    assert page.has_text?("Create your first agent")
  end


  test 'Button to delete agent is present' do
    go_to_agents_index
    first('.dropdown__trigger > button').click
    assert page.has_link?("Delete")
  end


  test 'Delete with confirmation' do
    before_count = Agent.count
    go_to_agents_index

    first('.dropdown__trigger > button').click
    click_link 'Delete'

    assert page.has_text?('Are you sure?')
    click_button('Delete')
    assert page.has_text?('Please enter the text exactly as it is displayed to confirm.')

    fill_in 'validation', with: 'dElEtE'
    click_button('Delete')
    assert page.has_text?('Please enter the text exactly as it is displayed to confirm.')

    fill_in 'validation', with: 'DELETE'
    click_button('Delete')
    assert page.has_text?('Agent with the name: My awesome weather bot has successfully been deleted.')

    assert_equal '/agents', current_path
    assert_equal before_count - 1, Agent.count
  end


  test 'Configure from index' do
    go_to_agents_index
    first('.dropdown__trigger > button').click
    click_link 'Configure'
    assert page.has_text?('Configure agent')
    fill_in 'Agent name', with: ''
    click_button 'Update'

    expected = ["Name can't be blank"]
    expected.each do |error|
      assert page.has_text?(error)
    end
    assert_equal 1, all('.help--error').size

    fill_in 'Agent name', with: 'My new updated agent'
    click_button 'Update'
    assert page.has_text?('Your agent has been succefully updated.')
    assert page.has_text?('My new updated agent')
  end

  test 'Agents can be found by name' do
    go_to_agents_index
    fill_in 'search_query', with: '800'
    click_button '#search'
    assert page.has_content?('T-800')
    assert page.has_no_content?('My awesome weather bot')
    assert_equal '/agents', current_path
  end

  test 'Agents can be found by agentname' do
    go_to_agents_index
    fill_in 'search_query', with: 'inator'
    click_button '#search'
    assert page.has_content?('T-800')
    assert page.has_no_content?('My awesome weather bot')
    assert_equal '/agents', current_path
  end

  test 'Empty search agent' do
    go_to_agents_index
    fill_in 'search_query', with: 'inator'
    click_button '#search'
    assert page.has_content?('T-800')
    assert page.has_no_content?('My awesome weather bot')
    fill_in 'search_query', with: ''
    click_button '#search'
    assert page.has_content?('T-800')
    assert page.has_content?('My awesome weather bot')
    assert_equal '/agents', current_path
  end
end
