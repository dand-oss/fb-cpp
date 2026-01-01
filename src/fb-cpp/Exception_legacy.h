/*
 * MIT License
 *
 * Copyright (c) 2025 Adriano dos Santos Fernandes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FBCPP_EXCEPTION_LEGACY_H
#define FBCPP_EXCEPTION_LEGACY_H

#include "fb-cpp_api.h"
#include "fb-api.h"
#include <stdexcept>
#include <string>
#include <cstdint>
#include <format>


///
/// fb-cpp namespace.
///
namespace fbcpp
{
	///
	/// Base exception class for all fb-cpp exceptions.
	/// Provides comprehensive debug info including origin, database URI, and error codes.
	///
	class FBCPP_API Exception : public std::runtime_error
	{
	public:
		///
		/// Constructs an Exception with the specified error message.
		///
		explicit Exception(const std::string& message)
			: std::runtime_error{message},
			  sqlCode_{0},
			  engineCode_{0}
		{
		}

		///
		/// Constructs an Exception from a Firebird status vector with context.
		///
		explicit Exception(const StatusVector& status, const std::string& context = "")
			: std::runtime_error{buildMessage(status, context, "")},
			  origin_{context},
			  sqlCode_{fbcpp::getSqlCode(status)},
			  engineCode_{fbcpp::getEngineCode(status)}
		{
		}

		///
		/// Constructs an Exception from a Firebird status vector with full context.
		/// @param status The Firebird status vector
		/// @param context The origin/context of the error (e.g., "Attachment::connect")
		/// @param uri The database URI or connection string
		///
		Exception(const StatusVector& status, const std::string& context, const std::string& uri)
			: std::runtime_error{buildMessage(status, context, uri)},
			  origin_{context},
			  uri_{uri},
			  sqlCode_{fbcpp::getSqlCode(status)},
			  engineCode_{fbcpp::getEngineCode(status)}
		{
		}

		///
		/// Returns the origin/context of the error.
		///
		const std::string& getOrigin() const noexcept
		{
			return origin_;
		}

		///
		/// Returns the database URI associated with this exception.
		///
		const std::string& getUri() const noexcept
		{
			return uri_;
		}

		///
		/// Returns the SQLCODE associated with this exception.
		///
		ISC_LONG getSqlCode() const noexcept
		{
			return sqlCode_;
		}

		///
		/// Returns the Firebird engine/GDS error code.
		///
		ISC_LONG getEngineCode() const noexcept
		{
			return engineCode_;
		}

	private:
		static std::string buildMessage(const StatusVector& status,
		                                const std::string& context,
		                                const std::string& uri)
		{
			std::string errorMsg = getErrorMessage(status);
			ISC_LONG sqlCode = fbcpp::getSqlCode(status);
			ISC_LONG engineCode = fbcpp::getEngineCode(status);

			std::string result;

			if (!context.empty())
				result = std::format("{}: ", context);

			if (!uri.empty())
				result += std::format("Database '{}' - ", uri);

			if (sqlCode != 0 || engineCode != 0)
				result += std::format("SQLCODE: {}, Engine: {} - ", sqlCode, engineCode);

			if (!errorMsg.empty())
				result += errorMsg;
			else if (result.empty())
				result = "Unknown database error";

			return result;
		}

		std::string origin_;
		std::string uri_;
		ISC_LONG sqlCode_;
		ISC_LONG engineCode_;
	};

	// Alias for backwards compatibility
	using FbCppException = Exception;
	using DatabaseException = Exception;

}  // namespace fbcpp


#endif  // FBCPP_EXCEPTION_LEGACY_H
