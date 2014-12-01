// ****************************************************************************
// daily/program_options/program_options.h
//
// Simple program options utility for when boost.program_options is too much
//
// Chris Glover (c.d.glover@gmail.com)
//
// June 8th, 2014
//
// ****************************************************************************
#pragma once
#ifndef _DAILY_PROGRAMOPTIONS_PROGRAMOPTIONS_H_
#define _DAILY_PROGRAMOPTIONS_PROGRAMOPTIONS_H_

#include <vector>
#include <sstream>
#include <stdexcept>

// ----------------------------------------------------------------------------
//
namespace daily {

// ----------------------------------------------------------------------------
//
class program_options
{
private:

	struct option_parser
	{
		explicit option_parser(char const* key)
			: key_(key)
		{}

		virtual ~option_parser()
		{}

		virtual bool parse_value(char const* value) = 0;

		char const* key_;
	};

	template<typename Value>
	struct option_holder : option_parser
	{
		option_holder(char const* key, Value& v)
			: option_parser(key)
			, v_(&v)
		{}

		virtual bool parse_value(char const* str_val)
		{
			std::istringstream ins;
			ins.str(str_val);
			ins >> std::boolalpha >> *v_;
			return ins.good();
		}

		Value* v_;
	};

public:

	program_options()
	{}

	~program_options()
	{
		typedef std::vector<option_parser*>::const_iterator c_iter;
		c_iter p_beg = parsers_.cbegin();
		c_iter p_end = parsers_.cend();

		for(c_iter p = p_beg; p != p_end; ++p)
		{
			delete *p;
		}
	}

	bool parse(int argc, char** argv, std::string& error)
	{
		return parse_impl(argc, argv, &error);
	}

	void parse(int argc, char** argv)
	{
		parse_impl(argc, argv, NULL);
	}

	template<typename T>
	void add(char const* key, T& destination_value)
	{
		parsers_.push_back(new option_holder<T>(key, destination_value));
	}

private:

	option_parser* get_parser(char const* key, std::size_t key_len)
	{
		typedef std::vector<option_parser*>::const_iterator c_iter;
		c_iter p_beg = parsers_.cbegin();
		c_iter p_end = parsers_.cend();

		for(c_iter p = p_beg; p != p_end; ++p)
		{
			if(std::strncmp((*p)->key_, key, key_len) == 0)
			{
				return *p;
			}
		}

		return NULL;
	}

	bool parse_impl(int argc, char** argv, std::string* error)
	{
		typedef std::vector<option_parser*>::const_iterator c_iter;
		c_iter p_beg = parsers_.cbegin();
		c_iter p_end = parsers_.cend();

		// Skip the program name.
		for(int i = 1; i < argc; ++i)
		{
			char const* key = argv[i];
			char const* val = std::strpbrk(argv[i], "=");
			option_parser* p = NULL;
			if(val)
			{
				p = get_parser(key, std::distance(key, val));
			}
			else
			{
				p = get_parser(key, std::strlen(key));
			}

			if(!p)
			{
				std::ostringstream e;
				e << "'" << key << "' is not a valid option.";
				if(error)
				{
					*error = e.str();
				}
				else
				{
					throw std::runtime_error(e.str());
				}

				return false;
			}				
				
			if(!p->parse_value(val+1))
			{
				std::ostringstream e;
				e << "Failed to parse value '" << val << "' for key '" << key;
				if(error)
				{
					*error = e.str();
				}
				else
				{
					throw std::runtime_error(e.str());
				}

				return false;
			}
		}

		return true;		
	}

	std::vector<option_parser*> parsers_;
};

}

#endif // _DAILY_PROGRAMOPTIONS_PROGRAMOPTIONS_H_