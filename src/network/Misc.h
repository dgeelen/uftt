#ifndef NET_MISC_H
#define NET_MISC_H

#include "../Types.h"

#include <vector>
#include <string>

#include <boost/asio/ip/address.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

#include "../util/StrFormat.h"

// TODO: give these a better place?

inline boost::asio::ip::address my_addr_from_string(const std::string& str)
{
	if (str == "255.255.255.255")
		return boost::asio::ip::address_v4::broadcast();
	else
		return boost::asio::ip::address::from_string(str);
}

inline std::string my_datetime_to_string(const boost::posix_time::ptime& td)
{
	return STRFORMAT("%04d-%02d-%02d %02d:%02d:%02d",
		td.date().year(),
		td.date().month(),
		td.date().day(),
		td.time_of_day().hours(),
		td.time_of_day().minutes(),
		td.time_of_day().seconds()
	);
}

inline uint32 pkt_get_uint32(uint8* buf)
{
	return (buf[0] << 0) | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

inline void pkt_put_uint32(uint32 val, uint8* buf)
{
	buf[0] = (val >>  0) & 0xFF;
	buf[1] = (val >>  8) & 0xFF;
	buf[2] = (val >> 16) & 0xFF;
	buf[3] = (val >> 24) & 0xFF;
}

inline void pkt_put_uint64(uint64 val, uint8* buf)
{
	buf[0] = (val >>  0) & 0xFF;
	buf[1] = (val >>  8) & 0xFF;
	buf[2] = (val >> 16) & 0xFF;
	buf[3] = (val >> 24) & 0xFF;
	buf[4] = (val >> 32) & 0xFF;
	buf[5] = (val >> 40) & 0xFF;
	buf[6] = (val >> 48) & 0xFF;
	buf[7] = (val >> 56 ) & 0xFF;
}

inline void pkt_put_vuint64(uint64 val, std::vector<uint8>& buf)
{
	while (val & ~0x7F) {
		buf.push_back((uint8)0x80 | (val & 0x7F));
		val >>= 7;
	}
	buf.push_back((uint8) val);
}

inline uint64 pkt_get_vuint64(std::vector<uint8>& buf, uint &idx)
{
	uint64 result = 0;
	uint8 shift = 0;
	uint8 val;
	do {
		val = buf[idx++];
		result |= (uint64(val&0x7F) << shift);
		shift += 7;
	} while (val & 0x80);
	return result;
}

inline uint64 pkt_get_vuint64(std::vector<uint8>& buf)
{
	uint idx = 0;
	return pkt_get_vuint64(buf, idx);
}

inline void pkt_put_vuint32(uint32 val, std::vector<uint8>& buf)
{
	while (val & ~0x7F) {
		buf.push_back((uint8)0x80 | (val & 0x7F));
		val >>= 7;
	}
	buf.push_back((uint8) val);
}

inline uint32 pkt_get_vuint32(std::vector<uint8>& buf, uint &idx)
{
	uint32 result = 0;
	uint8 shift = 0;
	uint8 val;
	do {
		val = buf[idx++];
		result |= (uint32(val&0x7F) << shift);
		shift += 7;
	} while (val & 0x80);
	return result;
}

inline uint32 pkt_get_vuint32(std::vector<uint8>& buf)
{
	uint idx = 0;
	return pkt_get_vuint32(buf, idx);
}

#endif//NET_MISC_H