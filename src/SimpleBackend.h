#ifndef SIMPLE_BACKEND_H
#define SIMPLE_BACKEND_H

#include "Types.h"

#define UFTT_PORT (47189)

#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/foreach.hpp>

#include "net-asio/asio_file_stream.h"
#include "net-asio/asio_http_request.h"

#include "SimpleConnection.h"

#include "AutoUpdate.h"

#include "../util/StrFormat.h"

class SimpleBackend {
	private:
		boost::asio::io_service service;
		services::diskio_service diskio;
		boost::asio::ip::udp::socket udpsocket;
		boost::asio::ip::tcp::acceptor tcplistener;
		SimpleTCPConnectionRef newconn;
		std::vector<SimpleTCPConnectionRef> conlist;
		std::map<std::string, boost::filesystem::path> sharelist;

		boost::thread servicerunner;

		void servicerunfunc() {
			boost::asio::io_service::work wobj(service);
			service.run();
		}

		void start_tcp_accept() {
			newconn = SimpleTCPConnectionRef(new SimpleTCPConnection(service, this));
			tcplistener.async_accept(newconn->socket,
				boost::bind(&SimpleBackend::handle_tcp_accept, this, boost::asio::placeholders::error));
		}

		void handle_tcp_accept(const boost::system::error_code& e) {
			if (e) {
				std::cout << "tcp accept failed: " << e.message() << '\n';
			} else {
				std::cout << "handling tcp accept\n";
				conlist.push_back(newconn);
				newconn->handle_tcp_accept();
				sig_new_upload("upload", conlist.size()-1);
				start_tcp_accept();
			}
		}

		uint8 udp_recv_buf[1024];
		boost::asio::ip::udp::endpoint udp_recv_addr;
		void start_udp_receive() {
			udpsocket.async_receive_from(boost::asio::buffer(udp_recv_buf), udp_recv_addr,
				boost::bind(&SimpleBackend::handle_udp_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
			);
		}

		void handle_udp_receive(const boost::system::error_code& e, std::size_t len) {
			if (!e) {
				if (len >= 4) {
					uint32 rpver = (udp_recv_buf[0] <<  0) | (udp_recv_buf[1] <<  8) | (udp_recv_buf[2] << 16) | (udp_recv_buf[3] << 24);
					if (len > 4) {
						std::cout << "got packet type " << (int)udp_recv_buf[4] << " from " << udp_recv_addr << "\n";
						switch (udp_recv_buf[4]) {
							case 1: if (rpver == 1) { // type = broadcast;
								std::set<uint32> versions;
								if (len > 5) {
									uint32 i = 6 + udp_recv_buf[5];
									if (i < len) {
										uint max = udp_recv_buf[i++];
										for (uint num = 0; num < max && i+8 <= len; ++num, i += 8) {
											uint32 lver = (udp_recv_buf[i+0] <<  0) | (udp_recv_buf[i+1] <<  8) | (udp_recv_buf[i+2] << 16) | (udp_recv_buf[i+3] << 24);
											uint32 hver = (udp_recv_buf[i+4] <<  0) | (udp_recv_buf[i+5] <<  8) | (udp_recv_buf[i+6] << 16) | (udp_recv_buf[i+7] << 24);
											for (uint32 j = lver; j <= hver; ++j)
												versions.insert(j);
										}
									}
								}
								if (versions.empty()) versions.insert(1);

								       if (versions.count(4)) {
									send_publishes(udp_recv_addr, 2, true);
								} else if (versions.count(3)) {
									send_publishes(udp_recv_addr, 0, true);
								} else if (versions.count(2)) {
									send_publishes(udp_recv_addr, 1, true);
								} else if (versions.count(1)) {
									send_publishes(udp_recv_addr, 1, len >= 6 && udp_recv_buf[5] > 0 && len-6 >= udp_recv_buf[5]);
								}
							}; break;
							case 2: { // type = reply;
								if (len >= 6) {
									uint32 slen = udp_recv_buf[5];
									if (len >= slen+6) {
										std::string sname((char*)udp_recv_buf+6, (char*)udp_recv_buf+6+slen);
										std::string surl = STRFORMAT("uftt-v%d://%s/%s", rpver, udp_recv_addr.address().to_string(), sname);
										sig_new_share(surl);
									}
									if (rpver == 1 && len > slen+6) {
										// a bit wasteful maybe?
										send_query(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT));
									}
								}
							}; break;
							case 3: { // type = autoupdate share
								if (len >= 6) {
									uint32 slen = udp_recv_buf[5];
									if (len >= slen+6) {
										std::string sname((char*)udp_recv_buf+6, (char*)udp_recv_buf+6+slen);
										std::string surl;
										surl += "uftt-v1://";
										surl += udp_recv_addr.address().to_string();
										surl += '/';
										surl += sname;
										sig_new_autoupdate(surl);
									}
								}
							}; break;
						}
					}
				}
			} else
				std::cout << "udp receive failed: " << e.message() << '\n';

			if (!e)
				start_udp_receive();
		}

		void send_query(boost::asio::ip::udp::endpoint ep) {
			uint8 udp_send_buf[1024];
			udp_send_buf[ 0] = 0x01;
			udp_send_buf[ 1] = 0x00;
			udp_send_buf[ 2] = 0x00;
			udp_send_buf[ 3] = 0x00;
			udp_send_buf[ 4] = 0x01;
			udp_send_buf[ 5] = 0x01;
			udp_send_buf[ 6] = 0x00;
			udp_send_buf[ 7] = 0x00;

			std::set<uint32> qversions;
			qversions.insert(1);
			qversions.insert(2);
			qversions.insert(4);

			int plen = 8;
			BOOST_FOREACH(uint32 ver, qversions) {
				udp_send_buf[plen+0] = udp_send_buf[plen+4] = (ver >>  0) & 0xff;
				udp_send_buf[plen+1] = udp_send_buf[plen+5] = (ver >>  8) & 0xff;
				udp_send_buf[plen+2] = udp_send_buf[plen+6] = (ver >> 16) & 0xff;
				udp_send_buf[plen+3] = udp_send_buf[plen+7] = (ver >> 24) & 0xff;
				++udp_send_buf[7];
				plen += 8;
			}

			boost::system::error_code err;
			if (ep.address() != boost::asio::ip::address_v4::broadcast())
				udpsocket.send_to(
					boost::asio::buffer(udp_send_buf, plen),
					ep,
					0
					,err
				);
			else
				this->send_udp_broadcast(udpsocket, boost::asio::buffer(udp_send_buf, plen), ep.port(), 0, err);

			if (err)
				std::cout << "query failed: " << err.message() << '\n';
		}

		void send_publish(const boost::asio::ip::udp::endpoint& ep, const std::string& name, int sharever, bool isbuild = false)
		{
			uint8 udp_send_buf[1024];
			udp_send_buf[0] = (sharever >>  0) & 0xff;
			udp_send_buf[1] = (sharever >>  8) & 0xff;
			udp_send_buf[2] = (sharever >> 16) & 0xff;
			udp_send_buf[3] = (sharever >> 24) & 0xff;
			udp_send_buf[4] = isbuild ? 3 : 2;
			udp_send_buf[5] = name.size();
			memcpy(&udp_send_buf[6], name.data(), name.size());
			udp_send_buf[name.size()+6] = 0;

			boost::system::error_code err;
			if (ep.address() != boost::asio::ip::address_v4::broadcast())
				udpsocket.send_to(
					boost::asio::buffer(udp_send_buf, name.size()+6),
					ep,
					0,
					err
				);
			else
				this->send_udp_broadcast(udpsocket, boost::asio::buffer(udp_send_buf, name.size()+6+1), ep.port(), 0, err);
			if (err)
				std::cout << "publish of '" << name << "' to '" << ep << "'failed: " << err.message() << '\n';
		}

		void send_publishes(const boost::asio::ip::udp::endpoint& ep, int sharever, bool sendbuilds) {
			typedef std::pair<const std::string, boost::filesystem::path> shareiter;
			if (sharever > 0)
				BOOST_FOREACH(shareiter& item, sharelist)
					if (item.first.size() < 0xff && !item.second.empty())
						send_publish(ep, item.first, sharever);

			if (sendbuilds)
				BOOST_FOREACH(const std::string& name, updateProvider.getAvailableBuilds())
					send_publish(ep, name, 1, true);
		}

		void add_local_share(std::string name, boost::filesystem::path path)
		{
			if (sharelist[name] != "")
				std::cout << "warning: replacing share with identical name\n";
			sharelist[name] = path;

			send_publish(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT), name, 1);
		}

		void download_share(std::string shareurl, boost::filesystem::path dlpath, boost::function<void(uint64,std::string,uint32)> handler)
		{
			size_t colonpos = shareurl.find_first_of(":");
			std::string proto = shareurl.substr(0, colonpos);
			uint32 version = atoi(proto.substr(6).c_str());
			shareurl.erase(0, proto.size()+3);
			size_t slashpos = shareurl.find_first_of("\\/");
			std::string host = shareurl.substr(0, slashpos);
			std::string share = shareurl.substr(slashpos+1);
			SimpleTCPConnectionRef newconn(new SimpleTCPConnection(service, this));
			newconn->sig_progress.connect(handler);
			conlist.push_back(newconn);
			boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(host), UFTT_PORT);
			newconn->socket.open(ep.protocol());
			std::cout << "Connecting...\n";
			newconn->socket.async_connect(ep, boost::bind(&SimpleBackend::dl_connect_handle, this, _1, newconn, share, dlpath, version));
		}

		void dl_connect_handle(const boost::system::error_code& e, SimpleTCPConnectionRef conn, std::string name, boost::filesystem::path dlpath, uint32 version)
		{
			if (e) {
				std::cout << "connect failed: " << e.message() << '\n';
			} else {
				std::cout << "Connected!\n";
				conn->handle_tcp_connect(name, dlpath, version);
			}
		}

		std::vector<boost::asio::ip::address> get_broadcast_adresses();

		template<typename BUF>
		void send_udp_broadcast(boost::asio::ip::udp::socket& sock, BUF buf, uint16 port, int flags, boost::system::error_code& err)
		{
			std::vector<boost::asio::ip::address> adresses;
			adresses = this->get_broadcast_adresses();
			BOOST_FOREACH(boost::asio::ip::address& addr, adresses) {
				udpsocket.send_to(
					buf,
					boost::asio::ip::udp::endpoint(addr, port),
					flags,
					err
				);
				if (err)
					std::cout << "broadcast to (" << addr << ") failed: " << err.message() << '\n';
			}
		}
		void attach_progress_handler(int num, boost::function<void(uint64,std::string,uint32)> handler)
		{
			conlist[num]->sig_progress.connect(handler);
		}

		void check_for_web_updates(boost::function<void(std::string, std::string)> handler)
		{
			std::string weburl = "http://hackykid.heliohost.org/site/autoupdate.php";
			//std::string weburl = "http://localhost:8080/site/autoupdate.php";

			boost::shared_ptr<boost::asio::http_request> request(new boost::asio::http_request(service, weburl));
			request->setHandler(boost::bind(&SimpleBackend::web_update_page_handler, this, boost::asio::placeholders::error, request, handler));
		}

		void web_update_page_handler(const boost::system::error_code& err, boost::shared_ptr<boost::asio::http_request> req, boost::function<void(std::string, std::string)> handler)
		{
			if (err) {
				std::cout << "Error checking for web updates: " << err.message() << '\n';
			} else
			{
				std::vector<std::pair<std::string, std::string> > builds = AutoUpdater::parseUpdateWebPage(req->getContent());
				for (uint i = 0; i < builds.size(); ++i)
					handler(builds[i].first, builds[i].second);
			}
		}

		void download_web_update(std::string url, boost::function<void(const boost::system::error_code&, boost::shared_ptr<boost::asio::http_request>) > handler)
		{
			boost::shared_ptr<boost::asio::http_request> request(new boost::asio::http_request(service, url));
			request->setHandler(boost::bind(handler, boost::asio::placeholders::error, request));
		}

	public:
		SimpleBackend()
			: diskio(service)
			, udpsocket(service)
			, tcplistener(service)
		{
			gdiskio = &diskio;
			udpsocket.open(boost::asio::ip::udp::v4());
			udpsocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address(), UFTT_PORT));
			udpsocket.set_option(boost::asio::ip::udp::socket::broadcast(true));

			tcplistener.open(boost::asio::ip::tcp::v4());
			tcplistener.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address(), UFTT_PORT));
			tcplistener.listen(16);

			// bind autoupdater
			updateProvider.newbuild.connect(
				boost::bind(&SimpleBackend::send_publish, this, boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT), _1, 0, true)
			);

			start_udp_receive();
			start_tcp_accept();

			boost::thread tt(boost::bind(&SimpleBackend::servicerunfunc, this));
			servicerunner.swap(tt);
		}

		void getsharepath(SimpleTCPConnection* conn, std::string name) {
			conn->sharepath = sharelist[name];
		}

		// the main public interface starts here...
		boost::signal<void(std::string)> sig_new_share;
		boost::signal<void(std::string,int)> sig_new_upload;
		boost::signal<void(std::string)> sig_new_autoupdate;
		boost::signal<void(std::string)> sig_download_ready;

		void slot_refresh_shares()
		{
			service.post(boost::bind(&SimpleBackend::send_query, this, boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT)));
		}

		void slot_add_local_share(std::string name, boost::filesystem::path path)
		{
			service.post(boost::bind(&SimpleBackend::add_local_share, this, name, path));
		}

		void slot_download_share(std::string shareurl, boost::filesystem::path dlpath, boost::function<void(uint64,std::string,uint32)> handler)
		{
			service.post(boost::bind(&SimpleBackend::download_share, this, shareurl, dlpath, handler));
		}

		void slot_attach_progress_handler(int num, boost::function<void(uint64,std::string,uint32)> handler)
		{
			service.post(boost::bind(&SimpleBackend::attach_progress_handler, this, num, handler));
		}

		void do_manual_query(std::string hostname) {
			service.post(boost::bind(&SimpleBackend::send_query, this,
				boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(hostname), UFTT_PORT)
			));
		}

		void do_manual_publish(std::string hostname) {
			service.post(boost::bind(&SimpleBackend::send_publishes, this,
				boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(hostname), UFTT_PORT)
			, true, true));
		}

		void do_check_for_web_updates(boost::function<void(std::string, std::string)> handler)
		{
			service.post(boost::bind(&SimpleBackend::check_for_web_updates, this, handler));
		}

		void do_download_web_update(std::string url, boost::function<void(const boost::system::error_code&, boost::shared_ptr<boost::asio::http_request>) > handler)
		{
			service.post(boost::bind(&SimpleBackend::download_web_update, this, url, handler));
		}
};

#endif//SIMPLE_BACKEND_H
