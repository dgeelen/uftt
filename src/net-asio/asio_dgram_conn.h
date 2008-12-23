#ifndef ASIO_DGRAM_CONN_H
#define ASIO_DGRAM_CONN_H

#include <queue>

#include <boost/asio.hpp>
#include <boost/utility.hpp>
#include <boost/bind.hpp>

#include "../Types.h"
#include "../util/asio_timer_oneshot.h"

namespace dgram {
	namespace timeout {
		const boost::asio::deadline_timer::duration_type small = boost::posix_time::milliseconds(750);          // 250ms
		const boost::asio::deadline_timer::duration_type stats = boost::posix_time::milliseconds(100);          // 100ms

		const boost::asio::deadline_timer::duration_type disconn = boost::posix_time::milliseconds(10*1000*60); // 10min
		const boost::asio::deadline_timer::duration_type active = boost::posix_time::milliseconds(5*1000);      // 5sec
		const boost::asio::deadline_timer::duration_type idle = boost::posix_time::milliseconds(1000*60);       // 1min

		const boost::posix_time::time_duration conn_reuse = boost::posix_time::minutes(10);
	}

	struct packet {
		enum constants {
			headersize = 4*4,
			//maxmtu = 2048,
			maxmtu = 1536,
			sendmtu = 1400,
			datasize = maxmtu-headersize,
			signiature = 0x02,
		};

		enum flags_enum {
			flag_ack  = 1 << 0,
			flag_psh  = 1 << 1, // do we need this?
			flag_rst  = 1 << 2,
			flag_syn  = 1 << 3,
			flag_fin  = 1 << 4,
			flag_ping = 1 << 5, // packet has no data, but sequence number should be increased by 1
			flag_oob  = 1 << 6, // packet wants no acknowledgement
		};

		uint8 header[4]; // includes flags, recvqid, and magic header signiature
		uint16 sendqid;  // senders qid
		uint16 window;
		uint32 seqnum;
		uint32 acknum;
		uint8 data[datasize];

		uint16 datalen;
		uint16 recvqid;  // recvers qid
		uint8 flags;
	};

	struct snditem {
		const void* data;
		size_t len;
		boost::function<void(const boost::system::error_code&, size_t len)> handler;

		snditem(const void* data_, size_t len_,  boost::function<void(const boost::system::error_code&, size_t len)> handler_)
			: data(data_), len(len_), handler(handler_) {};
	};

	struct rcvitem {
		void* data;
		size_t len;
		boost::function<void(const boost::system::error_code&, size_t len)> handler;

		rcvitem(void* data_, size_t len_,  boost::function<void(const boost::system::error_code&, size_t len)> handler_)
			: data(data_), len(len_), handler(handler_) {};
	};

	struct null_handler {
		void operator()(const  boost::system::error_code& ec, size_t len) {};

		typedef void result_type;
	};

	template<typename Proto>
	class conn_service;

	template<typename Proto>
	class conn : private boost::noncopyable {
		private:
			friend class conn_service<Proto>;

			enum state_enum {
				closed = 0,
				listening,
				synsent,
				synreceived,
				established,
				finwait1,
				finwait2,
				timewait,
				lastack,
			};

			boost::asio::io_service& service;
			conn_service<Proto>* cservice;

			uint8 rdata[packet::maxmtu];
			size_t rdsize;
			size_t rdoff;

			typename Proto::endpoint endpoint;

			packet sendpack;
			packet sendpackonce;

			int32 state;
			uint16 lqid;
			uint16 rqid;

			uint32 snd_una; // oldest unacknowledged sequence number
			uint32 snd_nxt; // next sequence number to be sent
			uint32 snd_wnd; // remote receiver window size

			uint32 rcv_nxt; // next sequence number expected on an incoming segment
			uint16 rcv_wnd; // receiver window size

			boost::asio::deadline_timer short_timer;
			boost::asio::deadline_timer long_timer;
			bool resending;

			std::deque<snditem> send_queue;
			std::deque<rcvitem> recv_queue;

			// stats
			uint32 resend;
			uint32 send_undelivered;
	public:
			// functions
			void initsendpack(packet& pack, uint16 datalen = 0, uint16 flags = packet::flag_ack) {
				pack.recvqid = rqid;
				pack.sendqid = lqid;
				pack.flags = flags;
				pack.seqnum = snd_una;
				pack.acknum = rcv_nxt;
				pack.window = rcv_wnd; // - received packets in queue
				pack.datalen = datalen; // default

				pack.header[0] = (pack.recvqid >> 8) & 0xff;
				pack.header[1] = (pack.recvqid >> 0) & 0xff;
				pack.header[2] = pack.flags;
				pack.header[3] = packet::signiature;
			}

			void check_queues()
			{
				check_recv_queues();
				check_send_queues();
			}

			void check_send_queues()
			{
				if (snd_una == snd_nxt && !send_queue.empty()) {
					snditem& front = send_queue.front();
					size_t trylen = front.len;
					if (trylen > packet::sendmtu)
						trylen = packet::sendmtu;
					initsendpack(sendpack, (uint16)trylen); // mtu always < 0xffff
					memcpy(sendpack.data,  front.data, trylen);
					sendpack.seqnum = snd_nxt++;
					send_packet();
					front.data = ((char*)front.data) + trylen;
					front.len -= trylen;
					send_undelivered += trylen;
					if (front.len == 0) {
						if (front.handler) {
							cservice->service.dispatch(boost::bind(front.handler, boost::system::error_code(), send_undelivered));
							send_undelivered = 0;
						}
						send_queue.pop_front();
					}
				}
			}

			void check_recv_queues()
			{
				if (rdsize == 0) return;

				if (!recv_queue.empty()) {
					#undef min
					size_t len = std::min(recv_queue.front().len, rdsize-rdoff);
					memcpy(recv_queue.front().data, rdata+rdoff, len);

					rdoff += len;

					if (rdsize == rdoff) {
						rdsize = 0;
						++rcv_nxt;
						initsendpack(sendpackonce);
						send_packet_once();
					}

					rcvitem ritem = recv_queue.front();
					recv_queue.pop_front();
					cservice->service.dispatch(boost::bind(ritem.handler, boost::system::error_code(), len));
				}

			}

			void send_packet_once() {
				send_packet_once(sendpackonce);
			}

			void send_packet_once(packet& pack) {
				//mcout(9) << STRFORMAT("[%3d,%3d] ", pack.sendid, pack.recvid) << "send udp packet: " << pack.type << "  len:" << pack.len << "\n";
				//socket->send_to(boost::asio::buffer(&pack, ipx_packet::headersize+pack.datalen), endpoint);
				cservice->socket.send_to(boost::asio::buffer(&pack, packet::headersize+pack.datalen), endpoint);
				//udp_sock->async_send_to(boost::asio::buffer(&sendpack, 4*5), udp_addr, boost::bind(&ignore));
			}

			void send_packet(const boost::system::error_code& error = boost::system::error_code(), bool newresend = true)
			{
				if (newresend) resending = true;
				if (!error && resending) {
					send_packet_once(sendpack);

					short_timer.expires_from_now(timeout::small);
					short_timer.async_wait(boost::bind(&conn<Proto>::send_packet, this, boost::asio::placeholders::error, false));
					++resend; // count all packets we send while using a timeout
				} else
					--resend; // and subtract all canceled timeouts
			}

			// for when a connection is established
			boost::function<void(const boost::system::error_code&)> handler;

			void handle_read(packet* pack, size_t len)
			{
				snd_wnd = pack->window;
				switch (state) {
					case closed: {
					}; break;
					case listening: {
						if (pack->flags & packet::flag_syn) {
							snd_nxt = snd_una+1;
							setstate(synreceived);
							initsendpack(sendpack, 0, packet::flag_syn | packet::flag_ack);
							send_packet();
						}
					}; break;
					case synsent:
					case synreceived: {
						if (pack->flags & packet::flag_ack && pack->acknum == snd_una+1) {
							++snd_una;
							initsendpack(sendpackonce);
							send_packet_once();
							setstate(established);
							cservice->service.dispatch(boost::bind(handler, boost::system::error_code()));
							handler.clear();
							check_queues();
						}
					}; break;
					case established: {
						if (pack->datalen > 0 && pack->seqnum == rcv_nxt) {
							if (!recv_queue.empty() && rdsize == 0) {
								if (recv_queue.front().len < pack->datalen) {
									memcpy(rdata, pack->data, pack->datalen);
									rdsize = pack->datalen;
									rdoff = 0;
								} else {
									memcpy(recv_queue.front().data, pack->data, pack->datalen);
									++rcv_nxt;
									cservice->service.dispatch(boost::bind(recv_queue.front().handler, boost::system::error_code(), pack->datalen));
									recv_queue.pop_front();
								}
							}
						}
						if (pack->flags & packet::flag_ack && pack->acknum == snd_una+1) {
							++snd_una;
							setstate(established);
						}
						check_queues();
						if (pack->seqnum != rcv_nxt) {
							initsendpack(sendpackonce);
							send_packet_once();
						}
					}; break;
					case finwait1: {
					}; break;
					case finwait2: {
					}; break;
					case timewait: {
					}; break;
					case lastack: {
					}; break;
				}
			}

			void setstate(uint32 newstate, bool resend=false) {
				state = newstate;
				short_timer.cancel();
				long_timer.cancel();
				resending = false;
			}

		public:
			conn(conn_service<Proto>* cservice_)
			: cservice(cservice_), short_timer(cservice_->service), long_timer(cservice_->service), service(cservice_->service)
			{
				rcv_wnd = 1;
				rdoff = rdsize = 0;
				cservice->addconn(this);
			}

			conn(conn_service<Proto>& cservice_)
			: cservice(&cservice_), short_timer(cservice_.service), long_timer(cservice_.service), service(cservice_.service)
			{
				rcv_wnd = 1;
				rdoff = rdsize = 0;
				cservice->addconn(this);
			}

			~conn()
			{
				cservice->delconn(this);
			}

			typename Proto::endpoint remote_endpoint() const {
				return endpoint;
			}

			template <typename MBS, typename Handler>
			void async_read_some(MBS& mbs, const Handler& handler)
			{
				void* buf;
				size_t buflen = 0;
				typename MBS::const_iterator iter = mbs.begin();
				typename MBS::const_iterator end = mbs.end();
				for (;buflen == 0 && iter != end; ++iter) {
					// at least 1 buffer
					boost::asio::mutable_buffer buffer(*iter);
					buf = boost::asio::buffer_cast<void*>(buffer);
					buflen = boost::asio::buffer_size(buffer);
				}
				if (buflen == 0) {
					// no-op
					service.dispatch(boost::bind<void>(handler, boost::system::error_code(), 0));
					return;
				};

				recv_queue.push_back(
					rcvitem(
						buf,
						buflen,
						handler
					)
				);
				check_queues();
			}

			template <typename CBS, typename Handler>
			void async_write_some(CBS& cbs, const Handler& handler)
			{
				typename CBS::const_iterator iter = cbs.begin();
				typename CBS::const_iterator end = cbs.end();
				size_t tlen = 0;
				for (;iter != end; ++iter) {
					// at least 1 buffer
					boost::asio::const_buffer buffer(*iter);
					const void* buf = boost::asio::buffer_cast<const void*>(buffer);
					size_t buflen = boost::asio::buffer_size(buffer);
					if (buflen > 0) {
						tlen += buflen;
						send_queue.push_back(
							snditem(
								buf,
								buflen,
								boost::function<void(const boost::system::error_code&, size_t len)>()
							)
						);
					}
				}
				if (tlen == 0)
					service.dispatch(boost::bind<void>(handler, boost::system::error_code(), 0));
				else
					send_queue.back().handler = handler;

				check_queues();
			}

			template <typename CBS>
			size_t write_some(CBS& cbs, boost::system::error_code& ec)
			{
				typename CBS::const_iterator iter = cbs.begin();
				typename CBS::const_iterator end = cbs.end();
				size_t tlen = 0;
				for (;iter != end; ++iter) {
					// at least 1 buffer
					boost::asio::const_buffer buffer(*iter);
					const void* buf = boost::asio::buffer_cast<const void*>(buffer);
					size_t buflen = boost::asio::buffer_size(buffer);
					if (buflen > 0) {
						tlen += buflen;
						send_queue.push_back(
							snditem(
								buf,
								buflen,
								boost::function<void(const boost::system::error_code&, size_t len)>()
							)
						);
					}
				}

				if (tlen != 0)
					send_queue.back().handler = null_handler();

				check_queues();

				return tlen;
			}

			template <typename ConnectHandler>
			void async_connect(const typename Proto::endpoint& endpoint_, const ConnectHandler& handler_)
			{
				snd_una = 111;
				handler = handler_;
				endpoint = endpoint_;
				state = synsent;
				snd_nxt = snd_una+1;
				rqid = 0xffff;
				initsendpack(sendpack, 0, packet::flag_syn);
				send_packet();
			}

			template <typename AcceptHandler>
			void async_accept(const AcceptHandler& handler_)
			{
				snd_una = 222;
				cservice->acceptlog.push_back(lqid);
				handler = handler_;
				state = listening;
			}

			template <typename AcceptHandler>
			void async_accept(typename Proto::endpoint& peer_endpoint, const AcceptHandler& handler)
			{
				peer_endpoint = endpoint;
				async_accept(handler);
			}

			void close()
			{
				// TODO: actually disconnect the socket
				setstate(closed);
			}
	};

	template<typename Proto>
	class conn_service {
		private:
			friend class conn<Proto>;
			boost::asio::io_service& service;
			typename Proto::socket& socket;

			std::vector<conn<Proto>*> conninfo;
			std::deque<size_t> acceptlog;

			void addconn(conn<Proto>* tconn)
			{
				for (size_t i = 0; i < conninfo.size(); ++i)
					if (!conninfo[i]) {
						conninfo[i] = tconn;
						tconn->lqid = i;
						return;
					}

				if (conninfo.size() == 0xffff) {
					tconn->lqid = 0;
					throw std::runtime_error("Too many connections\n");
				}

				tconn->lqid = conninfo.size();
				conninfo.push_back(tconn);
			}

			void delconn(conn<Proto>* tconn)
			{
				conninfo[tconn->lqid] = (conn<Proto>*)(((uintptr_t)0)-1);
				asio_timer_oneshot(service, timeout::conn_reuse, boost::bind(&conn_service<Proto>::clrconn, this, tconn->lqid));
			}

			void clrconn(size_t cqid)
			{
				conninfo[cqid] = NULL;
			}

			bool is_valid_idx(size_t idx) {
				if (idx >= conninfo.size())
					return false;

				if (conninfo[idx] == NULL)
					return false;

				if (conninfo[idx] == (conn<Proto>*)(((uintptr_t)0)-1))
					return false;

				return true;
			}

		public:
			conn_service(boost::asio::io_service& service_, typename Proto::socket& socket_)
			: service(service_), socket(socket_)
			{
			}

			void handle_read(uint8* pack, size_t len, typename Proto::endpoint* peer)
			{
				handle_read((packet*) pack, len, peer);
			}

			void handle_read(packet* pack, size_t len, typename Proto::endpoint* peer)
			{
				if (packet::headersize > len)
					return;

				if (pack->header[3] != packet::signiature)
					return;

				pack->recvqid = (pack->header[0] << 8) | (pack->header[1] << 0);
				pack->flags   = pack->header[2];
				pack->datalen = len - packet::headersize;

				conn<Proto>* tconn = NULL;
				if ((pack->flags & packet::flag_syn) && !(pack->flags & packet::flag_ack)) {
					while (!acceptlog.empty() && !is_valid_idx(acceptlog.front()))
						acceptlog.pop_front();

					if (!acceptlog.empty()) {
						tconn = conninfo[acceptlog.front()];
						acceptlog.pop_front();
					}
				} else if (is_valid_idx(pack->recvqid))
					tconn = conninfo[pack->recvqid];

				if (tconn) {
					if ((tconn->state == conn<Proto>::listening || tconn->state == conn<Proto>::synsent) && pack->flags & packet::flag_syn) {
						tconn->endpoint = *peer;
						tconn->rqid = pack->sendqid;
						tconn->rcv_nxt = pack->seqnum+1;
					}

					if (pack->sendqid == tconn->rqid && *peer == tconn->endpoint)
						tconn->handle_read(pack, len);
				}

			}
	};

};

#endif//ASIO_DGRAM_CONN_H