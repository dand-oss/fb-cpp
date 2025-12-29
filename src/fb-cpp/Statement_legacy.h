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

#ifndef FBCPP_STATEMENT_LEGACY_H
#define FBCPP_STATEMENT_LEGACY_H

#include "fb-api.h"
#include "Exception_legacy.h"
#include "Descriptor.h"
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>


namespace fbcpp
{
	class Attachment;
	class Transaction;

	///
	/// Date wrapper using std::chrono::year_month_day.
	///
	struct Date
	{
		std::chrono::year_month_day ymd;

		Date() = default;
		Date(std::chrono::year_month_day d) : ymd(d) {}  // Non-explicit for assignment compatibility
		explicit Date(ISC_DATE iscDate);

		// Allow assignment from year_month_day
		Date& operator=(std::chrono::year_month_day d) { ymd = d; return *this; }

		int year() const { return static_cast<int>(ymd.year()); }
		unsigned month() const { return static_cast<unsigned>(ymd.month()); }
		unsigned day() const { return static_cast<unsigned>(ymd.day()); }

		ISC_DATE toIscDate() const;
	};

	///
	/// Timestamp wrapper with date and time components.
	///
	struct Timestamp
	{
		Date date;
		std::chrono::hh_mm_ss<std::chrono::microseconds> time{std::chrono::microseconds{0}};

		Timestamp() = default;
		explicit Timestamp(ISC_TIMESTAMP iscTs);

		ISC_TIMESTAMP toIscTimestamp() const;
	};

	///
	/// Statement type enumeration matching Firebird's isc_info_sql_stmt_* values.
	///
	enum class StatementType : unsigned
	{
		SELECT = isc_info_sql_stmt_select,
		INSERT = isc_info_sql_stmt_insert,
		UPDATE = isc_info_sql_stmt_update,
		DELETE_ = isc_info_sql_stmt_delete,  // Trailing underscore avoids Windows DELETE macro
		DDL = isc_info_sql_stmt_ddl,
		GET_SEGMENT = isc_info_sql_stmt_get_segment,
		PUT_SEGMENT = isc_info_sql_stmt_put_segment,
		EXEC_PROCEDURE = isc_info_sql_stmt_exec_procedure,
		START_TRANSACTION = isc_info_sql_stmt_start_trans,
		COMMIT = isc_info_sql_stmt_commit,
		ROLLBACK = isc_info_sql_stmt_rollback,
		SELECT_FOR_UPDATE = isc_info_sql_stmt_select_for_upd,
		SET_GENERATOR = isc_info_sql_stmt_set_generator,
		SAVEPOINT = isc_info_sql_stmt_savepoint,
	};

	// SQL_DIALECT_V5, SQL_DIALECT_V6, SQL_DIALECT_CURRENT are defined in ibase.h

	///
	/// Represents a prepared SQL statement.
	///
	class Statement final
	{
	public:
		///
		/// Prepares an SQL statement.
		///
		explicit Statement(Attachment& attachment, Transaction& transaction, std::string_view sql);

		///
		/// Move constructor.
		///
		Statement(Statement&& o) noexcept
			: attachment{o.attachment},
			  handle{o.handle},
			  type_{o.type_},
			  inSqlda_{std::move(o.inSqlda_)},
			  outSqlda_{std::move(o.outSqlda_)},
			  cursorOpen_{o.cursorOpen_}
		{
			o.handle = 0;
			o.cursorOpen_ = false;
		}

		Statement& operator=(Statement&&) = delete;
		Statement(const Statement&) = delete;
		Statement& operator=(const Statement&) = delete;

		///
		/// Frees the statement.
		///
		~Statement() noexcept
		{
			if (isValid())
			{
				try
				{
					free();
				}
				catch (...)
				{
					// swallow
				}
			}
		}

	public:
		bool isValid() const noexcept { return handle != 0; }

		isc_stmt_handle getHandle() const noexcept { return handle; }

		Attachment& getAttachment() noexcept { return attachment; }

		StatementType getType() const noexcept { return type_; }

		///
		/// Returns the number of input parameters.
		///
		short getInputCount() const { return inSqlda_.count(); }

		///
		/// Returns the number of output columns.
		///
		short getOutputCount() const { return outSqlda_.count(); }

		///
		/// Returns the XSQLDA for input parameters.
		///
		XSqlDa& getInputSqlda() { return inSqlda_; }

		///
		/// Returns the XSQLDA for output columns.
		///
		XSqlDa& getOutputSqlda() { return outSqlda_; }

		///
		/// Executes the statement with the given transaction.
		/// Returns true if there's a result set available.
		///
		bool execute(Transaction& transaction);

		///
		/// Fetches the next row from the result set.
		/// Returns true if a row was fetched, false if no more rows.
		///
		bool fetchNext();

		///
		/// Closes any open cursor.
		///
		void closeCursor();

		///
		/// Frees the statement resources.
		///
		void free();

		///
		/// Returns the execution plan (legacy format).
		///
		std::string getPlan();

		///
		/// Returns the number of affected rows for INSERT/UPDATE/DELETE.
		///
		ISC_LONG getAffectedRows();

		// ========== Input parameter setters ==========

		void setNull(short index);

		void setShort(short index, short value);
		void setLong(short index, ISC_LONG value);
		void setInt64(short index, ISC_INT64 value);
		void setFloat(short index, float value);
		void setDouble(short index, double value);
		void setString(short index, std::string_view value);
		void setDate(short index, ISC_DATE value);
		void setTime(short index, ISC_TIME value);
		void setTimestamp(short index, const ISC_TIMESTAMP& value);

		// ========== Output column getters ==========

		bool isNull(short index) const;

		short getShort(short index) const;
		ISC_LONG getLong(short index) const;
		ISC_INT64 getInt64(short index) const;
		float getFloat(short index) const;
		double getDouble(short index) const;
		std::string getString(short index) const;
		ISC_DATE getDate(short index) const;
		ISC_TIME getTime(short index) const;
		ISC_TIMESTAMP getTimestamp(short index) const;

		///
		/// Returns column name by index (0-based).
		///
		std::string getColumnName(short index) const;

		///
		/// Returns column alias by index (0-based).
		///
		std::string getColumnAlias(short index) const;

		///
		/// Returns column type (SQL_* constant) by index (0-based).
		///
		short getColumnType(short index) const;

		// ========== Descriptor accessors (for compatibility with firebird2.cpp) ==========

		///
		/// Returns descriptors for all output columns.
		///
		std::vector<Descriptor> getOutputDescriptors() const;

		///
		/// Returns descriptors for all input parameters.
		///
		std::vector<Descriptor> getInputDescriptors() const;

		// ========== Optional-returning getters (for compatibility with firebird2.cpp) ==========
		// These overloads take unsigned index and return optional

		std::optional<bool> getBool(unsigned index) const;
		std::optional<int16_t> getInt16(unsigned index) const;
		std::optional<int32_t> getInt32(unsigned index) const;
		std::optional<int64_t> getInt64(unsigned index) const;
		std::optional<float> getFloat(unsigned index) const;
		std::optional<double> getDouble(unsigned index) const;
		std::optional<std::string> getString(unsigned index) const;
		std::optional<Date> getDate(unsigned index) const;
		std::optional<Timestamp> getTimestamp(unsigned index) const;

		// ========== Chrono-based setters (for compatibility with firebird2.cpp) ==========

		void setInt16(unsigned index, int16_t value);
		void setInt32(unsigned index, int32_t value);
		void setDate(unsigned index, std::chrono::year_month_day ymd);
		void setTimestamp(unsigned index, const Timestamp& ts);

	private:
		StatementType queryStatementType();

		Attachment& attachment;
		isc_stmt_handle handle = 0;
		StatementType type_ = StatementType::SELECT;
		XSqlDa inSqlda_;
		XSqlDa outSqlda_;
		bool cursorOpen_ = false;
	};

	///
	/// Execute a SQL statement immediately without preparing.
	///
	void executeImmediate(Attachment& attachment, Transaction& transaction, std::string_view sql);

}  // namespace fbcpp


#endif  // FBCPP_STATEMENT_LEGACY_H
