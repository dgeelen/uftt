#ifndef IPADDR_WATCHER_H
#define IPADDR_WATCHER_H

#include <boost/signals2.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#include <set>

class ipv4_watcher : private boost::noncopyable {
	private:
		class implementation;
		boost::shared_ptr<implementation> impl;
	public:
		ipv4_watcher(boost::asio::io_service& service);
		~ipv4_watcher();

		boost::signals2::signal<void(std::pair<boost::asio::ip::address,boost::asio::ip::address>)> add_addr;
		boost::signals2::signal<void(std::pair<boost::asio::ip::address,boost::asio::ip::address>)> del_addr;
		boost::signals2::signal<void(std::set<std::pair<boost::asio::ip::address,boost::asio::ip::address> >)> new_list;

		void async_wait();
};

class ipv6_watcher : private boost::noncopyable {
	private:
		class implementation;
		boost::shared_ptr<implementation> impl;
	public:
		ipv6_watcher(boost::asio::io_service& service);
		~ipv6_watcher();

		boost::signals2::signal<void(boost::asio::ip::address)> add_addr;
		boost::signals2::signal<void(boost::asio::ip::address)> del_addr;
		boost::signals2::signal<void(std::set<boost::asio::ip::address>)> new_list;

		void async_wait();
};

#endif//IPADDR_WATCHER_H
