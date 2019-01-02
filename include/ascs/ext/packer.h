/*
 * packer.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * packers
 */

#ifndef _ASCS_EXT_PACKER_H_
#define _ASCS_EXT_PACKER_H_

#include "ext.h"

namespace ascs { namespace ext {

class packer_helper
{
public:
	//return (size_t) -1 means length exceeded the ASCS_MSG_BUFFER_SIZE
	static size_t msg_size_check(size_t pre_len, const char* const pstr[], const size_t len[], size_t num)
	{
		if (nullptr == pstr || nullptr == len)
			return -1;

		auto total_len = pre_len;
		auto last_total_len = total_len;
		for (size_t i = 0; i < num; ++i)
			if (nullptr != pstr[i])
			{
				total_len += len[i];
				if (last_total_len > total_len || total_len > ASCS_MSG_BUFFER_SIZE) //overflow
				{
					unified_out::error_out("pack msg error: length exceeded the ASCS_MSG_BUFFER_SIZE!");
					return -1;
				}
				last_total_len = total_len;
			}

		return total_len;
	}
};

//protocol: length + body
class packer : public i_packer<std::string>
{
public:
	static size_t get_max_msg_size() {return ASCS_MSG_BUFFER_SIZE - ASCS_HEAD_LEN;}

	using i_packer<msg_type>::pack_msg;
	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		msg_type msg;
		auto pre_len = native ? 0 : ASCS_HEAD_LEN;
		auto total_len = packer_helper::msg_size_check(pre_len, pstr, len, num);
		if ((size_t) -1 == total_len)
			return msg;
		else if (total_len > pre_len)
		{
			if (!native)
			{
				auto head_len = (ASCS_HEAD_TYPE) total_len;
				if (total_len != head_len)
				{
					unified_out::error_out("pack msg error: length exceeded the header's range!");
					return msg;
				}

				head_len = ASCS_HEAD_H2N(head_len);
				msg.reserve(total_len);
				msg.append((const char*) &head_len, ASCS_HEAD_LEN);
			}
			else
				msg.reserve(total_len);

			for (size_t i = 0; i < num; ++i)
				if (nullptr != pstr[i])
					msg.append(pstr[i], len[i]);
		} //if (total_len > pre_len)

		return msg;
	}
	virtual msg_type pack_heartbeat()
	{
		ASCS_HEAD_TYPE head_len = ASCS_HEAD_LEN;
		head_len = ASCS_HEAD_H2N(head_len);

		return msg_type((const char*) &head_len, ASCS_HEAD_LEN);
	}

	//do not use following helper functions for heartbeat messages.
	virtual char* raw_data(msg_type& msg) const {return const_cast<char*>(std::next(msg.data(), ASCS_HEAD_LEN));}
	virtual const char* raw_data(msg_ctype& msg) const {return std::next(msg.data(), ASCS_HEAD_LEN);}
	virtual size_t raw_data_len(msg_ctype& msg) const {return msg.size() - ASCS_HEAD_LEN;}
};

//protocol: length + body
//T can be replaceable_buffer (an alias of auto_buffer) or shared_buffer, the latter makes output messages seemingly copyable.
template<typename T = replaceable_buffer>
class replaceable_packer : public i_packer<T>
{
private:
	typedef i_packer<T> super;

public:
	using super::pack_msg;
	virtual typename super::msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		auto raw_msg = new string_buffer();
		auto str = packer().pack_msg(pstr, len, num, native);
		raw_msg->swap(str);
		return typename super::msg_type(raw_msg);
	}
	virtual typename super::msg_type pack_heartbeat()
	{
		auto raw_msg = new string_buffer();
		auto str = packer().pack_heartbeat();
		raw_msg->swap(str);
		return typename super::msg_type(raw_msg);
	}

	virtual char* raw_data(typename super::msg_type& msg) const {return const_cast<char*>(std::next(msg.data(), ASCS_HEAD_LEN));}
	virtual const char* raw_data(typename super::msg_ctype& msg) const {return std::next(msg.data(), ASCS_HEAD_LEN);}
	virtual size_t raw_data_len(typename super::msg_ctype& msg) const {return msg.size() - ASCS_HEAD_LEN;}
};

//protocol: fixed length
class fixed_length_packer : public packer
{
public:
	using packer::pack_msg;
	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false) {return packer::pack_msg(pstr, len, num, true);}
	//not support heartbeat because fixed_length_unpacker cannot recognize heartbeat message

	virtual char* raw_data(msg_type& msg) const {return const_cast<char*>(msg.data());}
	virtual const char* raw_data(msg_ctype& msg) const {return msg.data();}
	virtual size_t raw_data_len(msg_ctype& msg) const {return msg.size();}
};

//protocol: [prefix] + body + suffix
class prefix_suffix_packer : public i_packer<std::string>
{
public:
	void prefix_suffix(const std::string& prefix, const std::string& suffix) {assert(!suffix.empty() && prefix.size() + suffix.size() < ASCS_MSG_BUFFER_SIZE); _prefix = prefix;  _suffix = suffix;}
	const std::string& prefix() const {return _prefix;}
	const std::string& suffix() const {return _suffix;}

public:
	using i_packer<msg_type>::pack_msg;
	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		msg_type msg;
		auto pre_len = native ? 0 : _prefix.size() + _suffix.size();
		auto total_len = packer_helper::msg_size_check(pre_len, pstr, len, num);
		if ((size_t) -1 == total_len)
			return msg;
		else if (total_len > pre_len)
		{
			msg.reserve(total_len);
			if (!native)
				msg.append(_prefix);
			for (size_t i = 0; i < num; ++i)
				if (nullptr != pstr[i])
					msg.append(pstr[i], len[i]);
			if (!native)
				msg.append(_suffix);
		} //if (total_len > pre_len)

		return msg;
	}

	virtual msg_type pack_heartbeat() {return _prefix + _suffix;}
	virtual char* raw_data(msg_type& msg) const {return const_cast<char*>(std::next(msg.data(), _prefix.size()));}
	virtual const char* raw_data(msg_ctype& msg) const {return std::next(msg.data(), _prefix.size());}
	virtual size_t raw_data_len(msg_ctype& msg) const {return msg.size() - _prefix.size() - _suffix.size();}

private:
	std::string _prefix, _suffix;
};

}} //namespace

#endif /* _ASCS_EXT_PACKER_H_ */
