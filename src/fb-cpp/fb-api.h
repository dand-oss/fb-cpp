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

#ifndef FBCPP_FB_API_H
#define FBCPP_FB_API_H

#if FB_CPP_LEGACY_API

// Firebird 2.5 Legacy C API
#include "ibase.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

///
/// fb-cpp namespace.
///
namespace fbcpp
{

/// ISC_STATUS array size
constexpr int STATUS_VECTOR_SIZE = 20;

/// Status vector type
using StatusVector = std::array<ISC_STATUS, STATUS_VECTOR_SIZE>;

/// SQL NULL indicator value
constexpr short SQL_NULL_FLAG = -1;

/// Check if status vector indicates an error
inline bool hasError(const StatusVector& status)
{
	return status[0] == 1 && status[1] > 0;
}

/// Get SQLCODE from status vector
inline ISC_LONG getSqlCode(const StatusVector& status)
{
	return isc_sqlcode(const_cast<ISC_STATUS*>(status.data()));
}

/// Get engine/GDS error code from status vector
inline ISC_LONG getEngineCode(const StatusVector& status)
{
	return (status[0] == 1) ? static_cast<ISC_LONG>(status[1]) : 0;
}

/// Get error message from status vector
inline std::string getErrorMessage(const StatusVector& status)
{
	std::string result;
	const ISC_STATUS* pvector = status.data();
	char buffer[512];

	while (fb_interpret(buffer, sizeof(buffer), &pvector))
	{
		if (!result.empty())
			result += "\n";
		result += buffer;
	}

	return result;
}

///
/// Database Parameter Block builder
///
class DPB
{
public:
	DPB()
	{
		buffer_.push_back(isc_dpb_version1);
	}

	void addString(unsigned char tag, const std::string& value)
	{
		buffer_.push_back(static_cast<char>(tag));
		buffer_.push_back(static_cast<char>(value.size()));
		buffer_.insert(buffer_.end(), value.begin(), value.end());
	}

	void addByte(unsigned char tag, unsigned char value)
	{
		buffer_.push_back(static_cast<char>(tag));
		buffer_.push_back(1);
		buffer_.push_back(static_cast<char>(value));
	}

	const char* data() const { return buffer_.data(); }
	short size() const { return static_cast<short>(buffer_.size()); }

private:
	std::vector<char> buffer_;
};

///
/// Transaction Parameter Block builder
///
class TPB
{
public:
	TPB()
	{
		buffer_.push_back(isc_tpb_version3);
	}

	void addTag(unsigned char tag)
	{
		buffer_.push_back(static_cast<char>(tag));
	}

	void addString(unsigned char tag, const std::string& value)
	{
		buffer_.push_back(static_cast<char>(tag));
		buffer_.push_back(static_cast<char>(value.size()));
		buffer_.insert(buffer_.end(), value.begin(), value.end());
	}

	const char* data() const { return buffer_.data(); }
	short size() const { return static_cast<short>(buffer_.size()); }

private:
	std::vector<char> buffer_;
};

///
/// XSQLDA wrapper for managing SQL descriptor areas
///
class XSqlDa
{
public:
	explicit XSqlDa(short numVars = 0)
	{
		resize(numVars);
	}

	~XSqlDa()
	{
		freeBuffers();
	}

	XSqlDa(const XSqlDa&) = delete;
	XSqlDa& operator=(const XSqlDa&) = delete;

	XSqlDa(XSqlDa&& other) noexcept
		: xsqlda_(other.xsqlda_),
		  buffers_(std::move(other.buffers_)),
		  nullIndicators_(std::move(other.nullIndicators_))
	{
		other.xsqlda_ = nullptr;
	}

	XSqlDa& operator=(XSqlDa&& other) noexcept
	{
		if (this != &other)
		{
			freeBuffers();
			delete[] reinterpret_cast<char*>(xsqlda_);
			xsqlda_ = other.xsqlda_;
			buffers_ = std::move(other.buffers_);
			nullIndicators_ = std::move(other.nullIndicators_);
			other.xsqlda_ = nullptr;
		}
		return *this;
	}

	void resize(short numVars)
	{
		freeBuffers();
		delete[] reinterpret_cast<char*>(xsqlda_);

		if (numVars <= 0)
		{
			xsqlda_ = nullptr;
			return;
		}

		size_t size = XSQLDA_LENGTH(numVars);
		xsqlda_ = reinterpret_cast<XSQLDA*>(new char[size]);
		std::memset(xsqlda_, 0, size);
		xsqlda_->version = SQLDA_VERSION1;
		xsqlda_->sqln = numVars;
		xsqlda_->sqld = 0;
	}

	void allocateBuffers()
	{
		if (!xsqlda_)
			return;

		freeBuffers();
		buffers_.resize(xsqlda_->sqld);
		nullIndicators_.resize(xsqlda_->sqld);

		for (short i = 0; i < xsqlda_->sqld; ++i)
		{
			XSQLVAR& var = xsqlda_->sqlvar[i];

			// Handle nullable types
			if ((var.sqltype & 1) == 0)
				var.sqltype |= 1;  // Make nullable

			// Allocate buffer based on type
			short dtype = var.sqltype & ~1;
			switch (dtype)
			{
				case SQL_TEXT:
					buffers_[i].resize(var.sqllen + 1);
					break;
				case SQL_VARYING:
					buffers_[i].resize(var.sqllen + sizeof(short) + 1);
					break;
				default:
					buffers_[i].resize(var.sqllen);
					break;
			}

			var.sqldata = buffers_[i].data();
			var.sqlind = &nullIndicators_[i];
		}
	}

	XSQLDA* get() { return xsqlda_; }
	const XSQLDA* get() const { return xsqlda_; }

	short count() const { return xsqlda_ ? xsqlda_->sqld : 0; }
	short allocated() const { return xsqlda_ ? xsqlda_->sqln : 0; }

	XSQLVAR& var(short index) { return xsqlda_->sqlvar[index]; }
	const XSQLVAR& var(short index) const { return xsqlda_->sqlvar[index]; }

	bool isNull(short index) const
	{
		return nullIndicators_[index] == SQL_NULL_FLAG;
	}

	void setNull(short index, bool null)
	{
		nullIndicators_[index] = null ? SQL_NULL_FLAG : 0;
	}

private:
	void freeBuffers()
	{
		buffers_.clear();
		nullIndicators_.clear();
	}

	XSQLDA* xsqlda_ = nullptr;
	std::vector<std::vector<char>> buffers_;
	std::vector<short> nullIndicators_;
};

}  // namespace fbcpp

#else  // FB_CPP_LEGACY_API

// Firebird 3.0+ OO API
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include "firebird/Interface.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

///
/// fb-cpp namespace.
///
namespace fbcpp
{
	namespace fb = Firebird;
}  // namespace fbcpp

#endif  // FB_CPP_LEGACY_API

#endif  // FBCPP_FB_API_H
