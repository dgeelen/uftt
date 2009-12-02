#include "UFTTSettings.h"

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

#include "Platform.h"

#include "util/Filesystem.h"

const int settings_version = 1;

UFTTSettings::UFTTSettings()
{
	loaded = false;

	posx  = posy  = 0;
	sizex = sizey = 0;

	dl_path = platform::getDownloadPathDefault();
	autoupdate = true;
	webupdateinterval = 2; // default to weekly update checks
	lastupdate = boost::posix_time::ptime(boost::posix_time::min_date_time);

	enablepeerfinder = true;
	laststuncheck = boost::posix_time::ptime(boost::posix_time::min_date_time);

	nickname = platform::getUserName();

	experimentalresume = true;

	traydoubleclick = true;

	/* Gtk GUI */
	show_toolbar = true;
	show_task_tray_icon = true;
	minimize_to_tray_mode = 1;
	start_in_tray = false;
}

bool UFTTSettings::load(boost::filesystem::path path_)
{
	path = path_;
	if (path.empty()) {
		platform::spathlist spl = platform::getSettingsPathList();
		BOOST_FOREACH(const platform::spathinfo& spi, spl) {
			if (!spi.second.empty() && ext::filesystem::exists(spi.second) && boost::filesystem::is_regular(spi.second)) {
				path = spi.second;
				break;
			}
		}

		if (path.empty())
			path = platform::getSettingsPathDefault().second;
	}
	try {
		std::ifstream ifs(path.native_file_string().c_str());
		if (!ifs.is_open()) return false;
		boost::archive::xml_iarchive ia(ifs);
		ia & NVP("settings", *this);
		loaded = true;
	} catch (std::exception& e) {
		std::cout << "Load settings failed: " << e.what() << '\n';
		loaded = false;
	}
	return loaded;
}

bool UFTTSettings::save()
{
	try {
		boost::filesystem::create_directories(path.branch_path());
		std::ofstream ofs(path.native_file_string().c_str());
		if (!ofs.is_open()) return false;
		boost::archive::xml_oarchive oa(ofs);
		oa & NVP("settings", *this);
		return true;
	} catch (std::exception& e) {
		std::cout << "Load settings failed: " << e.what() << '\n';
		return false;
	}
}
