require "application_system_test_case"

class StyleGuideTest < ApplicationSystemTestCase

  test "Navigate to style guide" do
    admin_login

    within(".nav") do
      click_link "Style guide"
    end

    within('.sg-intro') do
      assert page.has_text?("Icons")
    end

    click_link "Logo"

    within('.sg-intro') do
      assert page.has_text?("Logo")
    end
  end

end