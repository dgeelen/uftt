#include "Types.h"

// allow upx compression (prevents use of TLS callbacks in boost::thread)
extern "C" void tss_cleanup_implemented(void){}

//#define BOOST_ASIO_DISABLE_IOCP
//#define _WIN32_WINNT 0x0500
//#define WINVER 0x0500
//#define _WIN32_WINDOWS 0x0410
#define NOMINMAX

#include "BuildString.h"
#include "qt-gui/QTMain.h"
//#include "network/NetworkThread.h"
#include "net-asio/asio_ipx.h"
#include "net-asio/ipx_conn.h"
#include "SimpleBackend.h"
#include "AutoUpdate.h"
#include "UFTTSettings.h"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>

#include <fstream>

#include <string>

#include "Platform.h"

using namespace std;

using boost::asio::ipx;

// evil global variables
std::string thebuildstring;
services::diskio_service* gdiskio;
shared_vec exefile;
bool hassignedbuild(false);
AutoUpdater updateProvider;

#define BUFSIZE (1024*1024*16)
std::vector<uint8*> testbuffers;



			struct settrue {
				bool* value;
				settrue(bool* value_) : value(value_) {};
				void operator()(const boost::system::error_code& ec, const size_t len = 0)
				{
					boost::asio::detail::throw_error(ec);
					*value = true;
				}
				typedef void result_type;
			};

int runtest() {
	try {
		// TEST 0: all dependencies(dlls) have been loaded
		cout << "Loaded dll dependancies...Success\n";

		// todo, put something more here, like:
		cout << "Testing (a)synchronous network I/O...";
		{
			char sstr[] = "sendstring";
			char rstr[] = "1234567890";
			bool recvstarted = false;
			bool received = false;
			bool connected = false;
			bool accepted = false;
			bool sent = false;
			bool timedout = false;



			boost::asio::io_service service;
			boost::asio::ip::tcp::acceptor acceptor(service);
			boost::asio::ip::tcp::socket ssock(service);
			boost::asio::ip::tcp::socket rsock(service);

			acceptor.open(boost::asio::ip::tcp::v4());
			acceptor.bind(boost::asio::ip::tcp::endpoint(my_addr_from_string("127.0.0.1"), 23432));
			acceptor.listen(16);

			acceptor.async_accept(ssock, settrue(&accepted));

			rsock.open(boost::asio::ip::tcp::v4());
			rsock.async_connect(boost::asio::ip::tcp::endpoint(my_addr_from_string("127.0.0.1"), 23432), settrue(&connected));

			boost::asio::deadline_timer wd(service);
			wd.expires_from_now(boost::posix_time::seconds(10));
			wd.async_wait(settrue(&timedout));

			do {
				service.run_one();
				if (accepted && !sent) {
					// sync write here
					boost::asio::write(ssock, boost::asio::buffer(sstr));
					sent = true;
				}
				if (connected && !recvstarted) {
					// async read here
					boost::asio::async_read(rsock, boost::asio::buffer(rstr), settrue(&received));
					recvstarted = true;
				}
				//if (tries == 50 && !connected) {
				//	rsock.async_connect(acceptor.local_endpoint(), settrue(&connected));
				//}
			} while (!timedout && !received);
			//cout << "t: " << tries << '\n';
			//cout << "s: " << sstr << '\n';
			//cout << "r: " << rstr << '\n';
			if (timedout)
				cout << "Timed out...";
			if (!connected)
				throw std::runtime_error("connect failed");
			if (!accepted)
				throw std::runtime_error("accept failed");
			if (!sent)
				throw std::runtime_error("send failed");
			if (!recvstarted || !received)
				throw std::runtime_error("receive failed");
			if (string(sstr) != string(rstr))
				throw std::runtime_error("transfer failed");
		}
		cout << "Success\n";

		cout << "Testing udp broadcast...";
		{
			char sstr[] = "sendstring";
			//char rstr[] = "1234567890";
			boost::asio::io_service service;
			boost::asio::ip::udp::socket udpsocket(service);

			udpsocket.open(boost::asio::ip::udp::v4());
			//udpsocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address(), 23432));
			udpsocket.set_option(boost::asio::ip::udp::socket::broadcast(true));

			udpsocket.send_to(
				boost::asio::buffer(sstr),
				boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), 23432)
				//boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 23432),
				,0
			);


		}
		cout << "Success\n";

		cout << "Testing asynchronous disk I/O...";
		cout << "Skipped\n";

		cout << "Testing boost libraries...";
		cout << "Skipped\n";

		cout << "Testing qt libraries...";
		cout << "Skipped\n";

		return 0; // everything worked, success!
	} catch (std::exception& e) {
		cout << "Failed!\n";
		cout << "\n";
		cout << "Reason: " << e.what() << '\n';
		cout << flush;
		return 1; // error
	} catch (...) {
		cout << "Failed!\n";
		cout << "\n";
		cout << "Reason: Unknown\n";
		cout << flush;
		return 1; // error
	}
}

void calcbuildstring() {
	try {
		thebuildstring = string(BUILD_STRING);
		stringstream sstamp;

		int month, day, year;
		{
			char* temp = build_string_macro_date;

			// ANSI C Standard month names
			const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
			year = atoi(temp + 7);
			*(temp + 6) = 0;
			day = atoi(temp + 4);
			*(temp + 3) = 0;
			for (month = 0; month < 12; ++month)
				if (!strcmp(temp, months[month]))
					break;
			++month;
		}

		string tstamp(build_string_macro_time);
		BOOST_FOREACH(char& chr, tstamp)
			if (chr == ':') chr = '_';

		sstamp << year << '_' << month << '_' << day << '_' << tstamp;

		size_t pos = thebuildstring.find("<TIMESTAMP>");
		if (pos != string::npos) {
			thebuildstring.erase(pos, strlen("<TIMESTAMP>"));
			thebuildstring.insert(pos, sstamp.str());
		}
	} catch (...) {
	}
}

int imain( int argc, char **argv )
{
	bool madeConsole = false;
	if (argc > 1 && !platform::hasConsole()) {
		madeConsole = platform::newConsole();
		cout << "new console opened" << endl;
	}

	calcbuildstring();

	if (argc > 1 && string(argv[1]) == "--runtest")
		return runtest();

	if (argc > 2 && string(argv[1]) == "--replace")
		return AutoUpdater::replace(argv[0], argv[2]);

	// initialize backend & gui
	UFTTSettings settings;
	settings.load();

	QTMain gui(argc, argv, &settings);

	cout << "Build: " << thebuildstring << '\n';

	SimpleBackend backend(settings);
	gui.BindEvents(&backend);

	if (madeConsole)
		platform::freeConsole();

	// get services (gdiskio global evilly initialized by SimpleBackend constructror...)
	boost::asio::io_service& run_service  = gdiskio->get_io_service();
	boost::asio::io_service& work_service = gdiskio->get_work_service();

	// kick off some async tasks (which hijack the disk io thread)
	updateProvider.checkfile(*gdiskio, run_service, work_service, argv[0], thebuildstring, true);
	if (argc > 2 && string(argv[1]) == "--delete")
		AutoUpdater::remove(run_service, work_service, argv[2]);

	backend.slot_refresh_shares();

	backend.do_manual_publish("255.255.255.255");

	int ret = gui.run();

	settings.save();

	// hax...

	exit(ret);

	return ret;
}

int main( int argc, char **argv ) {
	int ret = -1;
	string message;
	try {
		ret = imain(argc, argv);
	} catch (std::exception& e) {
		message = string(e.what());
	} catch (...) {
		message = "unknown exception";
	}
	if (message != "") {
		if (!platform::hasConsole()) platform::newConsole();
		printf("fatal: %s\n", message.c_str());
	}
	if (ret != 0 && platform::hasConsole()) {
		printf("program aborted, termination in 30 seconds.\n");
		platform::msSleep(30*1000);
	}
	return ret;
}
