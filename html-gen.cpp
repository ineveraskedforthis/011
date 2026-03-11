#include "html-gen.hpp"
#include "chrono"
#include <format>
#include <string>
#include "simulation.hpp"
#include "url.hpp"

static std::string footer() {
	const auto now = std::chrono::system_clock::now();
	auto time_string = std::format("{:%Y-%m-%d %H:%M}", now);
	return  "<hr><footer> Report generated at <time>" + time_string + "</time> </footer>";
}

std::string resources_gacha(dcon::user_id user) {
	if(!user) {
		return "<html><head><title>Error</title></head><body>Invalid credentials</body></html>";
	}

	std::string result = "<html><head><title>RGO Acquisition</title></head><body>";

	result += "<a href="+ url_gen::main_mage() +">Back to the main page</a>";

	result += "<h1>Acquisition of Resource Gathering Operations</h1>";

	result += "<h2>Explanation</h2>In this world RGO lottery is the main way to distribute rights to exploit resources. Everyone who have managed to obtain Development Tickets is eligible to participate in the lottery.";

	result += "<h2>Tickets</h2>You possess " + std::to_string(pulls_count(user)) + " Development Tickets. Each draw requires at least 1 ticket.";

	result += "<h2>Draw</h2>";

	result += "<form action=\"" + url_gen::one_pull() + "\" method=\"post\"><button type=\"submit\">One draw</button></form>";
	result += "<form action=\"" + url_gen::ten_pull() + "\" method=\"post\"><button type=\"submit\">Ten draws</button></form>";

	result += footer();

	result += "</body></html>";
	return result;
}

std::string make_report(dcon::user_id user) {
	if(!user) {
		return "<html><head><title>Error</title></head><body>Invalid credentials</body></html>";
	}
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
	return "<html><head><title>Consent required.</title></head><body>We have to store your data to link your session cookie with in-game entity. We use data only for the in-game purposes. If you agree, pressing the  login button will generate a cookie and will allow you to interact with the game.<form action=\"" + url_gen::new_user() +"\" method=\"post\"><label for=\"name\">Username:</label><input type=\"text\" name=\"name\" required /><label for=\"password\">Password</label><input type=\"password\" name=\"password\" required /><input type=\"submit\" value=\"Sign in\"/></form></body></html>";
}