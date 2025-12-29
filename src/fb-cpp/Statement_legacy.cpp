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

#include "Statement_legacy.h"
#include "Attachment.h"
#include "Transaction.h"
#include <cstring>

using namespace fbcpp;


Statement::Statement(Attachment& attachment, Transaction& transaction, std::string_view sql)
	: attachment{attachment},
	  inSqlda_(10),   // Initial allocation for 10 params
	  outSqlda_(10)   // Initial allocation for 10 columns
{
	assert(attachment.isValid());
	assert(transaction.isValid());

	StatusVector status{};

	// Allocate statement handle
	isc_dsql_allocate_statement(status.data(), attachment.getHandlePtr(), &handle);
	if (hasError(status))
		throw Exception(status, "Failed to allocate statement");

	// Prepare the statement
	isc_dsql_prepare(
		status.data(),
		transaction.getHandlePtr(),
		&handle,
		static_cast<unsigned short>(sql.size()),
		const_cast<char*>(sql.data()),
		SQL_DIALECT_CURRENT,
		outSqlda_.get());

	if (hasError(status))
	{
		isc_dsql_free_statement(status.data(), &handle, DSQL_drop);
		handle = 0;
		throw Exception(status, "Failed to prepare statement");
	}

	// Get statement type
	type_ = queryStatementType();

	// Check for unsupported statement types
	switch (type_)
	{
		case StatementType::START_TRANSACTION:
			free();
			throw Exception("Cannot use SET TRANSACTION with Statement. Use Transaction class.");

		case StatementType::COMMIT:
			free();
			throw Exception("Cannot use COMMIT with Statement. Use Transaction::commit().");

		case StatementType::ROLLBACK:
			free();
			throw Exception("Cannot use ROLLBACK with Statement. Use Transaction::rollback().");

		default:
			break;
	}

	// Check if we need more output columns
	if (outSqlda_.get()->sqld > outSqlda_.allocated())
	{
		outSqlda_.resize(outSqlda_.get()->sqld);
		isc_dsql_describe(status.data(), &handle, SQL_DIALECT_CURRENT, outSqlda_.get());
		if (hasError(status))
			throw Exception(status, "Failed to describe output");
	}

	// Allocate output buffers
	if (outSqlda_.get()->sqld > 0)
		outSqlda_.allocateBuffers();

	// Describe input parameters
	isc_dsql_describe_bind(status.data(), &handle, SQL_DIALECT_CURRENT, inSqlda_.get());
	if (hasError(status))
		throw Exception(status, "Failed to describe input parameters");

	// Check if we need more input parameters
	if (inSqlda_.get()->sqld > inSqlda_.allocated())
	{
		inSqlda_.resize(inSqlda_.get()->sqld);
		isc_dsql_describe_bind(status.data(), &handle, SQL_DIALECT_CURRENT, inSqlda_.get());
		if (hasError(status))
			throw Exception(status, "Failed to describe input parameters");
	}

	// Allocate input buffers
	if (inSqlda_.get()->sqld > 0)
		inSqlda_.allocateBuffers();
}

StatementType Statement::queryStatementType()
{
	StatusVector status{};

	char infoRequest[] = {isc_info_sql_stmt_type};
	char infoBuffer[16];

	isc_dsql_sql_info(
		status.data(),
		&handle,
		sizeof(infoRequest),
		infoRequest,
		sizeof(infoBuffer),
		infoBuffer);

	if (hasError(status))
		throw Exception(status, "Failed to get statement type");

	// Parse the info buffer
	if (infoBuffer[0] == isc_info_sql_stmt_type)
	{
		short len = static_cast<short>(isc_vax_integer(&infoBuffer[1], 2));
		unsigned stmtType = static_cast<unsigned>(isc_vax_integer(&infoBuffer[3], len));
		return static_cast<StatementType>(stmtType);
	}

	return StatementType::SELECT;
}

bool Statement::execute(Transaction& transaction)
{
	assert(isValid());
	assert(transaction.isValid());

	// Close any existing cursor
	if (cursorOpen_)
		closeCursor();

	StatusVector status{};

	switch (type_)
	{
		case StatementType::SELECT:
		case StatementType::SELECT_FOR_UPDATE:
		{
			// Execute and open cursor
			isc_dsql_execute(
				status.data(),
				transaction.getHandlePtr(),
				&handle,
				SQL_DIALECT_CURRENT,
				inSqlda_.get()->sqld > 0 ? inSqlda_.get() : nullptr);

			if (hasError(status))
				throw Exception(status, "Failed to execute SELECT");

			cursorOpen_ = true;

			// Fetch first row
			return fetchNext();
		}

		case StatementType::EXEC_PROCEDURE:
		{
			// Execute procedure with possible output
			isc_dsql_execute2(
				status.data(),
				transaction.getHandlePtr(),
				&handle,
				SQL_DIALECT_CURRENT,
				inSqlda_.get()->sqld > 0 ? inSqlda_.get() : nullptr,
				outSqlda_.get()->sqld > 0 ? outSqlda_.get() : nullptr);

			if (hasError(status))
				throw Exception(status, "Failed to execute procedure");

			return outSqlda_.get()->sqld > 0;
		}

		default:
		{
			// Execute non-SELECT statement
			isc_dsql_execute(
				status.data(),
				transaction.getHandlePtr(),
				&handle,
				SQL_DIALECT_CURRENT,
				inSqlda_.get()->sqld > 0 ? inSqlda_.get() : nullptr);

			if (hasError(status))
				throw Exception(status, "Failed to execute statement");

			return false;
		}
	}
}

bool Statement::fetchNext()
{
	assert(isValid());

	if (!cursorOpen_)
		return false;

	StatusVector status{};

	ISC_STATUS fetchStatus = isc_dsql_fetch(
		status.data(),
		&handle,
		SQL_DIALECT_CURRENT,
		outSqlda_.get());

	if (fetchStatus == 100)  // No more rows
		return false;

	if (hasError(status))
		throw Exception(status, "Failed to fetch row");

	return true;
}

void Statement::closeCursor()
{
	if (!cursorOpen_)
		return;

	StatusVector status{};
	isc_dsql_free_statement(status.data(), &handle, DSQL_close);
	cursorOpen_ = false;

	// Ignore errors on cursor close
}

void Statement::free()
{
	if (!isValid())
		return;

	if (cursorOpen_)
		closeCursor();

	StatusVector status{};
	isc_dsql_free_statement(status.data(), &handle, DSQL_drop);
	handle = 0;

	if (hasError(status))
		throw Exception(status, "Failed to free statement");
}

std::string Statement::getPlan()
{
	assert(isValid());

	StatusVector status{};

	char infoRequest[] = {isc_info_sql_get_plan};
	char infoBuffer[4096];

	isc_dsql_sql_info(
		status.data(),
		&handle,
		sizeof(infoRequest),
		infoRequest,
		sizeof(infoBuffer),
		infoBuffer);

	if (hasError(status))
		throw Exception(status, "Failed to get plan");

	if (infoBuffer[0] == isc_info_sql_get_plan)
	{
		short len = static_cast<short>(isc_vax_integer(&infoBuffer[1], 2));
		return std::string(&infoBuffer[3], len);
	}

	return "";
}

ISC_LONG Statement::getAffectedRows()
{
	assert(isValid());

	StatusVector status{};

	char infoRequest[] = {isc_info_sql_records};
	char infoBuffer[64];

	isc_dsql_sql_info(
		status.data(),
		&handle,
		sizeof(infoRequest),
		infoRequest,
		sizeof(infoBuffer),
		infoBuffer);

	if (hasError(status))
		throw Exception(status, "Failed to get affected rows");

	ISC_LONG count = 0;

	if (infoBuffer[0] == isc_info_sql_records)
	{
		char* p = &infoBuffer[3];  // Skip header

		while (*p != isc_info_end)
		{
			char item = *p++;
			short len = static_cast<short>(isc_vax_integer(p, 2));
			p += 2;

			switch (item)
			{
				case isc_info_req_insert_count:
				case isc_info_req_update_count:
				case isc_info_req_delete_count:
					count += isc_vax_integer(p, len);
					break;
			}

			p += len;
		}
	}

	return count;
}

// ========== Input parameter setters ==========

void Statement::setNull(short index)
{
	inSqlda_.setNull(index, true);
}

void Statement::setShort(short index, short value)
{
	XSQLVAR& var = inSqlda_.var(index);
	*reinterpret_cast<short*>(var.sqldata) = value;
	inSqlda_.setNull(index, false);
}

void Statement::setLong(short index, ISC_LONG value)
{
	XSQLVAR& var = inSqlda_.var(index);
	*reinterpret_cast<ISC_LONG*>(var.sqldata) = value;
	inSqlda_.setNull(index, false);
}

void Statement::setInt64(short index, ISC_INT64 value)
{
	XSQLVAR& var = inSqlda_.var(index);
	*reinterpret_cast<ISC_INT64*>(var.sqldata) = value;
	inSqlda_.setNull(index, false);
}

void Statement::setFloat(short index, float value)
{
	XSQLVAR& var = inSqlda_.var(index);
	*reinterpret_cast<float*>(var.sqldata) = value;
	inSqlda_.setNull(index, false);
}

void Statement::setDouble(short index, double value)
{
	XSQLVAR& var = inSqlda_.var(index);
	*reinterpret_cast<double*>(var.sqldata) = value;
	inSqlda_.setNull(index, false);
}

void Statement::setString(short index, std::string_view value)
{
	XSQLVAR& var = inSqlda_.var(index);
	short dtype = var.sqltype & ~1;

	if (dtype == SQL_VARYING)
	{
		// VARCHAR: first 2 bytes are length
		short len = static_cast<short>(std::min(static_cast<size_t>(var.sqllen), value.size()));
		*reinterpret_cast<short*>(var.sqldata) = len;
		std::memcpy(var.sqldata + sizeof(short), value.data(), len);
	}
	else  // SQL_TEXT (CHAR)
	{
		short len = static_cast<short>(std::min(static_cast<size_t>(var.sqllen), value.size()));
		std::memcpy(var.sqldata, value.data(), len);
		// Pad with spaces
		if (len < var.sqllen)
			std::memset(var.sqldata + len, ' ', var.sqllen - len);
	}

	inSqlda_.setNull(index, false);
}

void Statement::setDate(short index, ISC_DATE value)
{
	XSQLVAR& var = inSqlda_.var(index);
	*reinterpret_cast<ISC_DATE*>(var.sqldata) = value;
	inSqlda_.setNull(index, false);
}

void Statement::setTime(short index, ISC_TIME value)
{
	XSQLVAR& var = inSqlda_.var(index);
	*reinterpret_cast<ISC_TIME*>(var.sqldata) = value;
	inSqlda_.setNull(index, false);
}

void Statement::setTimestamp(short index, const ISC_TIMESTAMP& value)
{
	XSQLVAR& var = inSqlda_.var(index);
	*reinterpret_cast<ISC_TIMESTAMP*>(var.sqldata) = value;
	inSqlda_.setNull(index, false);
}

// ========== Output column getters ==========

bool Statement::isNull(short index) const
{
	return outSqlda_.isNull(index);
}

short Statement::getShort(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return *reinterpret_cast<const short*>(var.sqldata);
}

ISC_LONG Statement::getLong(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return *reinterpret_cast<const ISC_LONG*>(var.sqldata);
}

ISC_INT64 Statement::getInt64(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return *reinterpret_cast<const ISC_INT64*>(var.sqldata);
}

float Statement::getFloat(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return *reinterpret_cast<const float*>(var.sqldata);
}

double Statement::getDouble(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return *reinterpret_cast<const double*>(var.sqldata);
}

std::string Statement::getString(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	short dtype = var.sqltype & ~1;

	if (dtype == SQL_VARYING)
	{
		short len = *reinterpret_cast<const short*>(var.sqldata);
		return std::string(var.sqldata + sizeof(short), len);
	}
	else  // SQL_TEXT
	{
		// Trim trailing spaces
		const char* p = var.sqldata + var.sqllen;
		while (p > var.sqldata && *(p - 1) == ' ')
			--p;
		return std::string(var.sqldata, p - var.sqldata);
	}
}

ISC_DATE Statement::getDate(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return *reinterpret_cast<const ISC_DATE*>(var.sqldata);
}

ISC_TIME Statement::getTime(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return *reinterpret_cast<const ISC_TIME*>(var.sqldata);
}

ISC_TIMESTAMP Statement::getTimestamp(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return *reinterpret_cast<const ISC_TIMESTAMP*>(var.sqldata);
}

std::string Statement::getColumnName(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return std::string(var.sqlname, var.sqlname_length);
}

std::string Statement::getColumnAlias(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return std::string(var.aliasname, var.aliasname_length);
}

short Statement::getColumnType(short index) const
{
	const XSQLVAR& var = outSqlda_.var(index);
	return var.sqltype & ~1;
}

// ========== Immediate execution ==========

void fbcpp::executeImmediate(Attachment& attachment, Transaction& transaction, std::string_view sql)
{
	StatusVector status{};

	isc_dsql_execute_immediate(
		status.data(),
		attachment.getHandlePtr(),
		transaction.getHandlePtr(),
		static_cast<unsigned short>(sql.size()),
		const_cast<char*>(sql.data()),
		SQL_DIALECT_CURRENT,
		nullptr);

	if (hasError(status))
		throw Exception(status, "Failed to execute immediate");
}

// ========== Date/Timestamp helpers ==========

// Firebird epoch is November 17, 1858 (Modified Julian Day 0)
// Julian day for 1858-11-17 is 2400001
static constexpr int FB_EPOCH_JULIAN = 2400001;

static int dateToJulian(int year, unsigned month, unsigned day)
{
	int a = (14 - static_cast<int>(month)) / 12;
	int y = year + 4800 - a;
	int m = static_cast<int>(month) + 12 * a - 3;
	return static_cast<int>(day) + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
}

static void julianToDate(int julian, int& year, unsigned& month, unsigned& day)
{
	int a = julian + 32044;
	int b = (4 * a + 3) / 146097;
	int c = a - (146097 * b) / 4;
	int d = (4 * c + 3) / 1461;
	int e = c - (1461 * d) / 4;
	int m = (5 * e + 2) / 153;
	day = static_cast<unsigned>(e - (153 * m + 2) / 5 + 1);
	month = static_cast<unsigned>(m + 3 - 12 * (m / 10));
	year = 100 * b + d - 4800 + m / 10;
}

Date::Date(ISC_DATE iscDate)
{
	int julian = iscDate + FB_EPOCH_JULIAN;
	int y;
	unsigned m, d;
	julianToDate(julian, y, m, d);
	ymd = std::chrono::year{y} / std::chrono::month{m} / std::chrono::day{d};
}

ISC_DATE Date::toIscDate() const
{
	int julian = dateToJulian(year(), month(), day());
	return julian - FB_EPOCH_JULIAN;
}

Timestamp::Timestamp(ISC_TIMESTAMP iscTs)
	: date(iscTs.timestamp_date)
{
	// ISC_TIME is in 100-microsecond units (10000 per second)
	auto totalMicros = std::chrono::microseconds{static_cast<int64_t>(iscTs.timestamp_time) * 100};
	time = std::chrono::hh_mm_ss<std::chrono::microseconds>{totalMicros};
}

ISC_TIMESTAMP Timestamp::toIscTimestamp() const
{
	ISC_TIMESTAMP result;
	result.timestamp_date = date.toIscDate();
	// Convert back to 100-microsecond units
	auto micros = time.to_duration();
	result.timestamp_time = static_cast<ISC_TIME>(micros.count() / 100);
	return result;
}

// ========== Descriptor accessors ==========

static Descriptor buildDescriptor(const XSQLVAR& var)
{
	Descriptor desc;
	short dtype = var.sqltype & ~1;

	desc.originalType = static_cast<DescriptorOriginalType>(dtype);
	desc.scale = var.sqlscale;
	desc.length = var.sqllen;
	desc.offset = 0;  // Not used in FB 2.5 C API
	desc.nullOffset = 0;  // Not used in FB 2.5 C API
	desc.isNullable = (var.sqltype & 1) != 0;
	desc.field = std::string(var.sqlname, var.sqlname_length);
	desc.alias = std::string(var.aliasname, var.aliasname_length);
	desc.relation = std::string(var.relname, var.relname_length);

	// Normalize to adjusted type
	switch (dtype)
	{
		case SQL_TEXT:
		case SQL_VARYING:
			desc.adjustedType = DescriptorAdjustedType::STRING;
			break;
		case SQL_SHORT:
			desc.adjustedType = DescriptorAdjustedType::INT16;
			break;
		case SQL_LONG:
			desc.adjustedType = DescriptorAdjustedType::INT32;
			break;
		case SQL_INT64:
			desc.adjustedType = DescriptorAdjustedType::INT64;
			break;
		case SQL_FLOAT:
			desc.adjustedType = DescriptorAdjustedType::FLOAT;
			break;
		case SQL_DOUBLE:
			desc.adjustedType = DescriptorAdjustedType::DOUBLE;
			break;
		case SQL_TIMESTAMP:
			desc.adjustedType = DescriptorAdjustedType::TIMESTAMP;
			break;
		case SQL_TYPE_DATE:
			desc.adjustedType = DescriptorAdjustedType::DATE;
			break;
		case SQL_TYPE_TIME:
			desc.adjustedType = DescriptorAdjustedType::TIME;
			break;
		case SQL_BLOB:
			desc.adjustedType = DescriptorAdjustedType::BLOB;
			break;
		default:
			desc.adjustedType = static_cast<DescriptorAdjustedType>(dtype);
			break;
	}

	return desc;
}

std::vector<Descriptor> Statement::getOutputDescriptors() const
{
	std::vector<Descriptor> result;
	short count = outSqlda_.count();
	result.reserve(count);

	for (short i = 0; i < count; ++i)
	{
		result.push_back(buildDescriptor(outSqlda_.var(i)));
	}

	return result;
}

std::vector<Descriptor> Statement::getInputDescriptors() const
{
	std::vector<Descriptor> result;
	short count = inSqlda_.count();
	result.reserve(count);

	for (short i = 0; i < count; ++i)
	{
		result.push_back(buildDescriptor(inSqlda_.var(i)));
	}

	return result;
}

// ========== Optional-returning getters ==========

std::optional<bool> Statement::getBool(unsigned index) const
{
	if (isNull(static_cast<short>(index)))
		return std::nullopt;
	return getShort(static_cast<short>(index)) != 0;
}

std::optional<int16_t> Statement::getInt16(unsigned index) const
{
	if (isNull(static_cast<short>(index)))
		return std::nullopt;
	return static_cast<int16_t>(getShort(static_cast<short>(index)));
}

std::optional<int32_t> Statement::getInt32(unsigned index) const
{
	if (isNull(static_cast<short>(index)))
		return std::nullopt;
	return static_cast<int32_t>(getLong(static_cast<short>(index)));
}

std::optional<int64_t> Statement::getInt64(unsigned index) const
{
	short idx = static_cast<short>(index);
	if (isNull(idx))
		return std::nullopt;
	return static_cast<int64_t>(getInt64(idx));
}

std::optional<float> Statement::getFloat(unsigned index) const
{
	short idx = static_cast<short>(index);
	if (isNull(idx))
		return std::nullopt;
	return getFloat(idx);
}

std::optional<double> Statement::getDouble(unsigned index) const
{
	short idx = static_cast<short>(index);
	if (isNull(idx))
		return std::nullopt;
	return getDouble(idx);
}

std::optional<std::string> Statement::getString(unsigned index) const
{
	short idx = static_cast<short>(index);
	if (isNull(idx))
		return std::nullopt;
	return getString(idx);
}

std::optional<Date> Statement::getDate(unsigned index) const
{
	short idx = static_cast<short>(index);
	if (isNull(idx))
		return std::nullopt;
	return Date(getDate(idx));
}

std::optional<Timestamp> Statement::getTimestamp(unsigned index) const
{
	short idx = static_cast<short>(index);
	if (isNull(idx))
		return std::nullopt;
	return Timestamp(getTimestamp(idx));
}

// ========== Chrono-based setters ==========

void Statement::setInt16(unsigned index, int16_t value)
{
	setShort(static_cast<short>(index), static_cast<short>(value));
}

void Statement::setInt32(unsigned index, int32_t value)
{
	setLong(static_cast<short>(index), static_cast<ISC_LONG>(value));
}

void Statement::setDate(unsigned index, std::chrono::year_month_day ymd)
{
	Date d(ymd);
	setDate(static_cast<short>(index), d.toIscDate());
}

void Statement::setTimestamp(unsigned index, const Timestamp& ts)
{
	setTimestamp(static_cast<short>(index), ts.toIscTimestamp());
}
