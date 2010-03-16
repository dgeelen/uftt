#ifndef UFTT_SETTINGS_H
#define UFTT_SETTINGS_H

#include "Types.h"

#include <vector>
#include <set>

#include <boost/filesystem/path.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>

#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/version.hpp>

#define NVP(x,y) (boost::serialization::make_nvp(x,y))

struct vector_as_string {
	std::vector<uint8>& data;

	vector_as_string(std::vector<uint8>& data_)
		: data(data_) {};

	template<class Archive>
	void save(Archive & ar, const unsigned int version) const {
		std::string strdata(data.begin(), data.end());
		ar & NVP("data", strdata);
	}

	template<class Archive>
	void load(Archive & ar, const unsigned int version) const {
		std::string strdata;
		ar & NVP("data", strdata);
		data.swap(std::vector<uint8>(strdata.begin(), strdata.end()));
	}

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) const {
		boost::serialization::split_member(ar, *this, version);
	}
};

class UFTTSettings {
	public:
		UFTTSettings();

		bool load(boost::filesystem::path path_ = "");
		bool save();

	public:
		boost::filesystem::path path;
		bool loaded;

		std::vector<uint8> dockinfo;
		int posx;
		int posy;
		int sizex;
		int sizey;

		boost::filesystem::path dl_path;
		bool autoupdate;
		bool experimentalresume;
		bool traydoubleclick;

		int webupdateinterval; // 0:never, 1:daily, 2:weekly, 3:monthly
		boost::posix_time::ptime lastupdate;

		bool enablepeerfinder;
		std::string nickname;

		boost::posix_time::ptime laststuncheck;
		std::string stunpublicip;

		/* Gtk GUI */
		bool show_toolbar;
		bool show_task_tray_icon;
		// 0 = Do not minimize to tray,
		// 1 = Minimize to tray on close,
		// 2 = Minimize to tray on minimize.
		// Option 2 may not be implemented for all platforms/gui combinations.
		int  minimize_to_tray_mode;
		bool start_in_tray;

		// Automatically clear completed tasks from the tasklist after
		// this amount of time. Any negative duration indicates that
		// the option is disabled.
		boost::posix_time::time_duration auto_clear_tasks_after;

	public:
		template<class Archive>
		void serialize(Archive & ar, const unsigned int version) {
			if (version  <  4) ar & NVP("dockinfo", dockinfo);
			ar & NVP("posx"    , posx);
			ar & NVP("posy"    , posy);
			ar & NVP("sizex"   , sizex);
			ar & NVP("sizey"   , sizey);
			if (version >=  2) ar & NVP("downloadpath", dl_path);
			if (version >=  3) ar & NVP("autoupdate"  , autoupdate);
			if (version >=  5) ar & NVP("updateinterval", webupdateinterval);
			if (version >=  6) ar & NVP("peerfinder", enablepeerfinder);
			if (version >=  7) ar & NVP("experimentalresume", experimentalresume);
			if (version >=  8) ar & NVP("nickname", nickname);
			if (version >= 11) ar & NVP("traydoubleclick", traydoubleclick);

			if (version >=  5) ar & NVP("lastupdate", lastupdate);
			if (version >=  6 && version <=  8) {
				boost::posix_time::ptime lastpeerquery;
				boost::posix_time::ptime prevpeerquery;
				std::set<std::string> foundpeers;
				ar & NVP("lastpeerquery", lastpeerquery);
				ar & NVP("prevpeerquery", prevpeerquery);
				ar & NVP("foundpeers", foundpeers);
			}
			if (version >=  4) ar & NVP("dockinfo", dockinfo);

			if (version <=  9) { // force new defaults for old versions
				experimentalresume = true;
				autoupdate = true;
			}

			//if (version >=  4 && version < 6) ar & NVP("dockinfo", dockinfo);
			//if (version >=  6) ar & NVP("dockinfo", vector_as_string(dockinfo));

			/* Gtk GUI */
			if (version >= 12) ar & NVP("show_toolbar", show_toolbar);
			if (version >= 13) ar & NVP("show_task_tray_icon", show_task_tray_icon);
			if (version >= 13) ar & NVP("minimize_to_tray_mode", minimize_to_tray_mode);
			if (version >= 13) ar & NVP("start_in_tray", start_in_tray);
			if (version >= 14) ar & NVP("auto_clear_tasks_after", auto_clear_tasks_after);
		}
};
typedef boost::shared_ptr<UFTTSettings> UFTTSettingsRef;

BOOST_CLASS_VERSION(UFTTSettings, 14)

///////////////////////////////////////////////////////////////////////////////
//  Serialization support for boost::filesystem::path
namespace boost { namespace serialization {

	template<class Archive>
inline void save (Archive & ar, boost::filesystem::path const& p,
    const unsigned int /* file_version */)
{
    using namespace boost::serialization;
    std::string path_str(p.string());
    ar & make_nvp("path", path_str);
}

template<class Archive>
inline void load (Archive & ar, boost::filesystem::path &p,
    const unsigned int /* file_version */)
{
    using namespace boost::serialization;
    std::string path_str;
    ar & make_nvp("path", path_str);
    p = boost::filesystem::path(path_str);//, boost::filesystem::native);
}

// split non-intrusive serialization function member into separate
// non intrusive save/load member functions
template<class Archive>
inline void serialize (Archive & ar, boost::filesystem::path &p,
    const unsigned int file_version)
{
    boost::serialization::split_free(ar, p, file_version);
}

} } // namespace boost::serialization

#endif//UFTT_SETTINGS_H
