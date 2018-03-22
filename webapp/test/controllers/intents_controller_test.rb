require 'test_helper'

class IntentsControllerTest < ActionDispatch::IntegrationTest

  #
  # Index intents access
  #
  test "Index intents access" do
    sign_in users(:admin)
    get user_agent_intents_url(users(:admin), agents(:weather))
    assert_response :success
    assert_nil flash[:alert]
  end

  test "Index intents forbidden" do
    sign_in users(:confirmed)
    get user_agent_intents_url(users(:admin), agents(:weather))
    assert_equal "Unauthorized operation.", flash[:alert]
  end


  #
  # Show
  #
  test 'Show intent access' do
    sign_in users(:show_on_agent_weather)
    get user_agent_intent_url(users(:show_on_agent_weather), agents(:weather), intents(:weather_forecast))
    assert_response :success
    assert_nil flash[:alert]
  end

  test 'Show intent forbidden' do
    sign_in users(:confirmed)
    get user_agent_intent_url(users(:admin), agents(:weather), intents(:weather_forecast))
    assert_redirected_to agents_url
    assert_equal 'Unauthorized operation.', flash[:alert]
  end


  #
  # Create
  #
  test 'Create access' do
    sign_in users(:edit_on_agent_weather)
    post user_agent_intents_url(users(:edit_on_agent_weather), agents(:weather)),
         params: {
           intent: { intentname: 'my_new_intent', description: 'A new intent' },
           format: :json
         }
    assert_redirected_to user_agent_intents_path(users(:edit_on_agent_weather), agents(:weather))
    assert_nil flash[:alert]
  end

  test 'Create forbidden' do
    sign_in users(:show_on_agent_weather)
    post user_agent_intents_url(users(:show_on_agent_weather), agents(:weather)),
         params: {
           intent: { intentname: 'my_new_intent', description: 'A new intent' },
           format: :json
         }
    assert_response :forbidden
    assert response.body.include?('Unauthorized operation.')
  end

  #
  # Update
  #
  test 'Update access' do
    sign_in users(:edit_on_agent_weather)
    patch user_agent_intent_url(users(:edit_on_agent_weather), agents(:weather), intents(:weather_forecast)),
          params: {
            intent: { intentname: 'my_new_name', description: 'The new intent name' },
            format: :json
          }
    assert_redirected_to user_agent_intents_path(users(:edit_on_agent_weather), agents(:weather))
    assert_nil flash[:alert]
  end

  test 'Update forbidden' do
    sign_in users(:show_on_agent_weather)
    patch user_agent_intent_url(users(:show_on_agent_weather), agents(:weather), intents(:weather_forecast)),
          params: {
            intent: { intentname: 'my_new_name', description: 'The new intent name' },
            format: :json
          }
    assert_response :forbidden
    assert response.body.include?('Unauthorized operation.')
  end

  #
  # Confirm_destroy access
  #
  test 'Confirm delete access' do
    sign_in users(:admin)
    get user_agent_intent_confirm_destroy_url(users(:edit_on_agent_weather), agents(:weather), intents(:weather_forecast))
    assert_response :success
    assert_nil flash[:alert]
  end

  test 'Confirm delete forbidden' do
    sign_in users(:show_on_agent_weather)
    get user_agent_intent_confirm_destroy_url(users(:show_on_agent_weather), agents(:weather), intents(:weather_forecast))
    assert_redirected_to agents_url
    assert_equal 'Unauthorized operation.', flash[:alert]
  end

  #
  # Delete
  #
  test 'Delete access' do
    sign_in users(:admin)
    delete user_agent_intent_url(users(:admin), agents(:weather), intents(:weather_forecast))
    assert_redirected_to user_agent_intents_path(users(:admin), agents(:weather))
    assert_nil flash[:alert]
  end

  test 'Delete forbidden' do
    sign_in users(:show_on_agent_weather)
    delete user_agent_intent_url(users(:show_on_agent_weather), agents(:weather), intents(:weather_forecast))
    assert_redirected_to agents_url
    assert_equal 'Unauthorized operation.', flash[:alert]
  end

  #
  # Add locale
  #
  test 'Add locale' do
    sign_in users(:edit_on_agent_weather)
    post user_agent_intent_add_locale_url(users(:admin), agents(:weather), intents(:weather_forecast), locale_to_add: 'fr')
    assert_redirected_to user_agent_intent_path(users(:admin), agents(:weather), intents(:weather_forecast), locale: 'fr')
    assert_nil flash[:alert]
  end

  test 'Add locale forbidden' do
    sign_in users(:show_on_agent_weather)
    post user_agent_intent_add_locale_url(users(:admin), agents(:weather), intents(:weather_forecast), locale_to_add: 'fr')
    assert_redirected_to agents_url
    assert_equal 'Unauthorized operation.', flash[:alert]
  end

  #
  # Remove locale
  #
  test 'Remove locale' do
    sign_in users(:edit_on_agent_weather)
    delete user_agent_intent_remove_locale_url(users(:admin), agents(:weather), intents(:weather_forecast), locale_to_remove: 'fr')
    assert_redirected_to user_agent_intent_path(users(:admin), agents(:weather), intents(:weather_forecast), locale: 'en')
    assert_nil flash[:alert]
  end

  test 'Remove  locale forbidden' do
    sign_in users(:show_on_agent_weather)
    delete user_agent_intent_remove_locale_url(users(:admin), agents(:weather), intents(:weather_forecast), locale_to_remove: 'fr')
    assert_redirected_to agents_url
    assert_equal 'Unauthorized operation.', flash[:alert]
  end


  #
  # Scope agent on owner
  #
  test 'Intent agent scoped on current user' do
    sign_in users(:confirmed)
    post user_agent_intents_url(users(:confirmed), agents(:weather_confirmed)),
         params: {
           intent: { intentname: 'my_new_intent', description: 'A new intent' },
           format: :json
         }
    assert_redirected_to user_agent_intents_path(users(:confirmed), agents(:weather_confirmed))
    assert_nil flash[:alert]
  end


  #
  # List available destinations
  #
  test 'Allow intent available destinations' do
    sign_in users(:admin)

    get available_destinations_user_agent_intent_url(users(:admin), agents(:weather), intents(:weather_forecast))
    assert_response :success
    assert_nil flash[:alert]
  end

  test 'Forbid intent available destinations' do
    sign_in users(:confirmed)

    get available_destinations_user_agent_intent_url(users(:admin), agents(:weather), intents(:weather_forecast))
    assert_redirected_to agents_url
    assert_equal 'Unauthorized operation.', flash[:alert]
  end
end
