#pragma once

#include <string>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace Pengine::Utils
{
	namespace detail
	{

		template<typename T>
		inline std::string ToStr(const T& arg)
		{
			if constexpr (std::is_same_v<T, std::string>)
			{
				return arg;
			}
			else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>)
			{
				return std::string(arg);
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				return arg ? "true" : "false";
			}
			else
			{
				std::ostringstream oss;
				oss << arg;
				return oss.str();
			}
		}

		inline void FormatImpl(std::ostringstream& oss, const std::string& fmt, size_t pos)
		{
			while (pos < fmt.size())
			{
				if (fmt[pos] == '{' && pos + 1 < fmt.size() && fmt[pos + 1] == '}')
				{
					throw std::runtime_error("Too few arguments provided to format()");
				}
				if (fmt[pos] == '{' && pos + 1 < fmt.size() && fmt[pos + 1] == '{')
				{
					oss << '{';
					pos += 2;
				}
				else if (fmt[pos] == '}' && pos + 1 < fmt.size() && fmt[pos + 1] == '}')
				{
					oss << '}';
					pos += 2;
				}
				else
				{
					oss << fmt[pos++];
				}
			}
		}

		template<typename T, typename... Args>
		inline void FormatImpl(std::ostringstream& oss, const std::string& fmt, size_t pos,
			const T& first, const Args&... rest)
		{
			while (pos < fmt.size())
			{
				if (fmt[pos] == '{' && pos + 1 < fmt.size() && fmt[pos + 1] == '{')
				{
					oss << '{';
					pos += 2;
				}
				else if (fmt[pos] == '}' && pos + 1 < fmt.size() && fmt[pos + 1] == '}')
				{
					oss << '}';
					pos += 2;
				}
				else if (fmt[pos] == '{' && pos + 1 < fmt.size() && fmt[pos + 1] == '}')
				{
					oss << ToStr(first);
					FormatImpl(oss, fmt, pos + 2, rest...);
					return;
				}
				else
				{
					oss << fmt[pos++];
				}
			}
		}

	}

	template<typename... Args>
	inline std::string Format(const std::string& fmt, const Args&... args)
	{
		std::ostringstream oss;
		detail::FormatImpl(oss, fmt, 0, args...);
		return oss.str();
	}

}
