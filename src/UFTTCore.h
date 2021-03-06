#ifndef UFTT_CORE_H
#define UFTT_CORE_H

#include "Types.h"

#include <string>
#include <map>
#include <vector>

#include <boost/asio.hpp>
#include "net-asio/asio_file_stream.h"

#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <memory>

#include "util/Filesystem.h"
#include "util/SignalConnection.h"

#include "UFTTSettings.h"
#include "UFTTCommandLine.h"

#define UFTT_PORT (47189)

struct LocalShareID {
	std::string sid;
};

struct LocalShareInfo {
	LocalShareID id;
	ext::filesystem::path path;

};

struct ShareID {
	uint32 mid;
	std::string sid; // id as string for first step

	bool operator==(const ShareID& o) const
	{
		return (this->sid == o.sid) && (this->mid == o.mid);
	}

	bool operator!=(const ShareID& o) const
	{
		return !(this->operator==(o));
	}

	bool operator<(const ShareID& o) const
	{
		if (mid != o.mid) return mid < o.mid;
		return sid < o.sid;
	}
};

struct ShareInfo {
	ShareID id;
	std::string name;
	std::string host;
	std::string proto;
	std::string user;
	bool isupdate;
	ShareInfo() : isupdate(false) {};
};

struct TaskID {
	uint32 mid;
	uint32 cid;
};

enum TaskStatus {
	TASK_STATUS_INITIAL,
	TASK_STATUS_ENQUEUED,
	TASK_STATUS_CONNECTING,
	TASK_STATUS_CONNECTED,
	TASK_STATUS_TRANSFERRING,
	TASK_STATUS_COMPLETED,
	TASK_STATUS_ERROR,
};

struct TaskInfo {
	TaskID id;
	ShareID shareid;
	ShareInfo shareinfo;
	bool isupload;
	TaskStatus status;
	std::string getTaskStatusString() const {
		switch(status) {
			case TASK_STATUS_INITIAL:
				return "Initializing";
			case TASK_STATUS_ENQUEUED:
				return "Enqueued";
			case TASK_STATUS_CONNECTING:
				return "Connecting";
			case TASK_STATUS_CONNECTED:
				return "Connected";
			case TASK_STATUS_TRANSFERRING:
				return "Transferring";
			case TASK_STATUS_COMPLETED:
				return "Completed";
			case TASK_STATUS_ERROR:
				return "Error: " + error_message;
		}
		return "Invalid";
	};
	std::string error_message;
	ext::filesystem::path path;   // Path that this Task is being downloaded to / uploaded from
	uint64 transferred;
	uint64 size;
	uint32 queue;

	boost::posix_time::ptime         start_time;           // Time that the transfer was first started
	boost::posix_time::ptime         last_progress_report; // Last time that a sig_progress was issued for this TaskInfo
	boost::posix_time::ptime         last_rtt_update;      // When we last checked the rtt (not implemented yet)
	boost::posix_time::time_duration rtt;                  // Estimated round-trip-time for this connection (not implemented yet)
	uint64                           speed;                // Estimated number of bytes we are currently transferring per second

	TaskInfo()
	: isupload(true)
	, status(TASK_STATUS_INITIAL)
	, transferred(0)
	, size(0)
	, queue(0)
	, start_time(boost::posix_time::not_a_date_time)
	, last_progress_report(boost::posix_time::not_a_date_time)
	, last_rtt_update(boost::posix_time::not_a_date_time)
	, rtt(boost::posix_time::seconds(-1))
	, speed(0)
	{
	};
};

class UFTTCore {
	public:
		enum GuiCommand {
			GUI_CMD_SHOW = 0,
			GUI_CMD_QUIT,
		};
	private:
		// declare these first so they will be destroyed last
		boost::asio::io_service io_service;

		boost::asio::ip::tcp::acceptor local_listener;
		std::string mwid;

		std::map<std::string, ext::filesystem::path> localshares;
		std::vector<std::shared_ptr<class INetModule> > netmodules;
		UFTTSettingsRef settings;

		boost::thread servicerunner;

		boost::signals2::signal<void(GuiCommand)> sigGuiCommand;
		void servicerunfunc();

		void handleArgsDone(CommandLineInfo* cmdinfo);

		// private asio coroutines
		struct LocalConnectionHandler;
		struct LocalConnectionWriter;
		struct CommandExecuteHelper;
	public:
		UFTTCore(UFTTSettingsRef settings_, const CommandLineInfo& cmdinfo);
		~UFTTCore();

		void initialize();
		void handleArgs(CommandLineInfo* cmdinfo);

		// Core->Gui commands
		SignalConnection connectSigGuiCommand(const boost::function<void(GuiCommand)>& cb);
		void setMainWindowId(const std::string& mwid_);

		// Local Share management
		void addLocalShare(const std::string& name, const ext::filesystem::path& path);
		void addLocalShare(const ext::filesystem::path& path); ///< Determines share name based on the path
		bool isLocalShare(const std::string& name);
		bool getLocalShareID(const std::string& name, LocalShareID* id);
		void delLocalShare(const LocalShareID& id);

		void connectSigAddLocalShare(const LocalShareInfo& info);
		void connectSigDelLocalShare(const LocalShareID& id);

		ext::filesystem::path getLocalSharePath(const LocalShareID& id);
		ext::filesystem::path getLocalSharePath(const std::string& id);

		std::vector<std::string> getLocalShares();

		// Remote Share Listing/Downloading
		SignalConnection connectSigAddShare(const boost::function<void(const ShareInfo&)>& cb);
		SignalConnection connectSigDelShare(const boost::function<void(const ShareID&)>& cb);
		SignalConnection connectSigNewTask(const boost::function<void(const TaskInfo& tinfo)>& cb);
		SignalConnection connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb);

		void startDownload(const ShareID& sid, const ext::filesystem::path& path);


		// Misc network functions
		void doManualPublish(const std::string& host);
		void doManualQuery(const std::string& host);
		void doRefreshShares();


		// Deprecated but still in use
		UFTTSettingsRef getSettingsRef();

		// Service getters
		boost::asio::io_service& get_io_service();
		boost::asio::io_service& get_work_service();

		// Slightly hacky error passing to GUI
		int error_state; // 0 == none, 1 == warning, 2 == error
		std::string error_string;
};
#endif//UFTT_CORE_H
