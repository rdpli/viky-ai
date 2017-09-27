require "application_system_test_case"

class ProfileTest < ApplicationSystemTestCase

  test "Edit profile access" do
    admin_login
    within(".nav") do
      click_link "admin"
    end
    click_link "Edit your profile"

    assert page.has_content?("Authentication parameters")
  end


  test "Profile change Name, username, bio" do
    admin_login
    within(".nav") do
      click_link "admin"
    end
    click_link "Edit your profile"

    fill_in 'Name', with: 'Batman and Robin'
    fill_in 'Username', with: 'batman'
    fill_in 'Bio', with: "blah blah blah"

    click_button 'Update profile'

    assert page.has_field?('Name', with: 'Batman and Robin')
    assert page.has_field?('Username', with: 'batman')
    assert page.has_field?('Bio', with: 'blah blah blah')
  end


  test "Profile add avatar and remove" do
    admin_login
    within(".nav") do
      click_link "admin"
    end
    click_link "Edit your profile"

    within("main") do
      assert find(".avatar img")['src'].include? 'default'
    end

    file = File.join(Rails.root, 'test', 'fixtures', 'files', 'avatar_upload.png')
    attach_file('profile_image', file)

    click_button 'Update profile'

    within("main") do
      assert find(".avatar img")['src'].include? 'square'
    end

    assert page.has_field?('profile[remove_image]')

    check('Remove avatar')
    click_button 'Update profile'
    within("main") do
      assert find(".avatar img")['src'].include? 'default'
    end
  end


  test "Profile avatar with not permitted format" do
    admin_login
    within(".nav") do
      click_link "admin"
    end
    click_link "Edit your profile"

    within("main") do
      assert find(".avatar img")['src'].include? 'default'
    end

    file = File.join(Rails.root, 'test', 'fixtures', 'files', 'avatar_upload.txt')
    attach_file('profile_image', file)

    click_button 'Update profile'

    within("main") do
      assert find(".avatar img")['src'].include? 'default'
    end

    assert page.has_content?("Image isn't of allowed type")
  end


  test "Update Authentication parameters without any change" do
    admin_login
    within(".nav") do
      click_link "admin"
    end
    click_link "Edit your profile"

    fill_in 'Password', with: ''
    click_button 'Update profile'
    assert page.has_content?("Authentication parameters")
    assert !page.has_content?("Password is too short (minimum is 6 characters)")
  end


  test "Update Authentication parameters - Change password" do
    admin_login
    within(".nav") do
      click_link "admin"
    end
    click_link "Edit your profile"

    fill_in 'Password', with: 'short'
    click_button 'Update profile'
    assert page.has_content?("Authentication parameters")
    assert page.has_content?("Password is too short (minimum is 6 characters)")

    fill_in 'Password', with: 'shortshort'
    click_button 'Update profile'
    assert page.has_content?("Authentication parameters")
    assert !page.has_content?("Password is too short (minimum is 6 characters)")

    first('.nav__footer svg').click # Logout
    click_link "Log in"
    fill_in 'Email', with: 'admin@voqal.ai'
    fill_in 'Password', with: 'shortshort'
    click_button 'Log in'

    assert page.has_content?("Signed in successfully.")
  end


  test "Update Authentication parameters - Change email" do
    admin_login
    within(".nav") do
      click_link "admin"
    end
    click_link "Edit your profile"

    fill_in 'Email', with: 'admin_new@voqal.ai'
    click_button 'Update profile'

    assert page.has_content?("Currently waiting confirmation for: admin_new@voqal.ai")
  end


  test "Delete my account" do
    admin_login
    within(".nav") do
      click_link "admin"
    end
    click_link "Edit your profile"

    click_link 'I want delete my account'

    assert page.has_content?("Are you sure?")

    fill_in 'validation', with: 'DEL'
    click_button('Delete my account')
    assert page.has_content?('Please enter the text exactly as it is displayed to confirm.')


    fill_in 'validation', with: 'DELETE'
    click_button('Delete my account')

    assert page.has_content?('Your account has been succefully delete. ByeBye.')
  end

end