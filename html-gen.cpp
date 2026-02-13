#include "html-gen.hpp"
#include "chrono"
#include <format>
#include "simulation.hpp"
#include "url.hpp"

static std::string footer() {
	const auto now = std::chrono::system_clock::now();
	auto time_string = std::format("{:%Y-%m-%d %H:%M}", now);
	return  "<footer> Report generated at <time>" + time_string + "</time> </footer>";
}

std::string make_report(dcon::user_id user) {
	if(!user) {
		return "<html><head><title>Error</title></head><body>Invalid credentials</body></html>";
	}
	const auto now = std::chrono::system_clock::now();
	auto time_string = std::format("{:%Y-%m-%d %H:%M}", now);
	return std::format(
		"<html><head><title>Control panel</title></head><body><h1>Welcome, {}</h1> {}<h2>Available building types</h2>{}{}{}</body></html>",
		retrieve_user_name(user),
		retrieve_user_report_body(user),
		retrieve_building_type_list(),
		trade_section(user),
		footer()
	);
}

std::string login_page() {
	return "<html><head><title>Consent required.</title></head><body>We have to store your data to link your session cookie with in-game entity. We use data only for the in-game purposes. If you agree, pressing the  login button will generate a cookie and will allow you to interact with the game.<form action=\"" + url_gen::new_user() +"\" method=\"post\"><label for=\"name\">Username:</label><input type=\"text\" name=\"name\" required /><label for=\"password\">Password</label><input type=\"password\" name=\"password\" required /><input type=\"submit\" value=\"Sign in\"/></form> </body></html>";
}