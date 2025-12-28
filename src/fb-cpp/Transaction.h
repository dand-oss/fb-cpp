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

#ifndef FBCPP_TRANSACTION_H
#define FBCPP_TRANSACTION_H

#include "fb-api.h"
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>


///
/// fb-cpp namespace.
///
namespace fbcpp
{
	class Attachment;
	class Client;

	///
	/// Transaction isolation level.
	///
	enum class TransactionIsolationLevel
	{
		CONSISTENCY,
		READ_COMMITTED,
		SNAPSHOT
	};

	///
	/// Transaction read committed mode.
	///
	enum class TransactionReadCommittedMode
	{
		NO_RECORD_VERSION,
		RECORD_VERSION
	};

	///
	/// Transaction access mode.
	///
	enum class TransactionAccessMode
	{
		READ_ONLY,
		READ_WRITE
	};

	///
	/// Transaction wait mode.
	///
	enum class TransactionWaitMode
	{
		NO_WAIT,
		WAIT
	};

	///
	/// Represents options used when creating a Transaction object.
	///
	class TransactionOptions final
	{
	public:
		const std::vector<std::uint8_t>& getTpb() const { return tpb; }

		TransactionOptions& setTpb(const std::vector<std::uint8_t>& value)
		{
			tpb = value;
			return *this;
		}

		TransactionOptions& setTpb(std::vector<std::uint8_t>&& value)
		{
			tpb = std::move(value);
			return *this;
		}

		const std::optional<TransactionIsolationLevel> getIsolationLevel() const { return isolationLevel; }

		TransactionOptions& setIsolationLevel(TransactionIsolationLevel value)
		{
			isolationLevel = value;
			return *this;
		}

		const std::optional<TransactionReadCommittedMode> getReadCommittedMode() const { return readCommittedMode; }

		TransactionOptions& setReadCommittedMode(TransactionReadCommittedMode value)
		{
			readCommittedMode = value;
			return *this;
		}

		const std::optional<TransactionAccessMode> getAccessMode() const { return accessMode; }

		TransactionOptions& setAccessMode(TransactionAccessMode value)
		{
			accessMode = value;
			return *this;
		}

		const std::optional<TransactionWaitMode> getWaitMode() const { return waitMode; }

		TransactionOptions& setWaitMode(TransactionWaitMode value)
		{
			waitMode = value;
			return *this;
		}

		bool getNoAutoUndo() const { return noAutoUndo; }

		TransactionOptions& setNoAutoUndo(bool value)
		{
			noAutoUndo = value;
			return *this;
		}

		bool getIgnoreLimbo() const { return ignoreLimbo; }

		TransactionOptions& setIgnoreLimbo(bool value)
		{
			ignoreLimbo = value;
			return *this;
		}

		bool getRestartRequests() const { return restartRequests; }

		TransactionOptions& setRestartRequests(bool value)
		{
			restartRequests = value;
			return *this;
		}

		bool getAutoCommit() const { return autoCommit; }

		TransactionOptions& setAutoCommit(bool value)
		{
			autoCommit = value;
			return *this;
		}

	private:
		std::vector<std::uint8_t> tpb;
		std::optional<TransactionIsolationLevel> isolationLevel;
		std::optional<TransactionReadCommittedMode> readCommittedMode;
		std::optional<TransactionAccessMode> accessMode;
		std::optional<TransactionWaitMode> waitMode;
		bool noAutoUndo = false;
		bool ignoreLimbo = false;
		bool restartRequests = false;
		bool autoCommit = false;
	};

	///
	/// Transaction state for tracking lifecycle.
	///
	enum class TransactionState
	{
		ACTIVE,
		PREPARED,
		COMMITTED,
		ROLLED_BACK
	};

	///
	/// Represents a transaction in a Firebird database.
	///
	class Transaction final
	{
	public:
		///
		/// Constructs a Transaction object that starts a transaction in the specified
		/// Attachment using the specified options.
		///
		explicit Transaction(Attachment& attachment, const TransactionOptions& options = {});

		///
		/// Move constructor.
		///
		Transaction(Transaction&& o) noexcept
			: attachment_{o.attachment_},
			  handle_{o.handle_},
			  state_{o.state_}
		{
			o.handle_ = 0;
			o.state_ = TransactionState::ROLLED_BACK;
		}

		Transaction& operator=(Transaction&&) = delete;
		Transaction(const Transaction&) = delete;
		Transaction& operator=(const Transaction&) = delete;

		///
		/// Rolls back the transaction if it is still active.
		///
		~Transaction() noexcept
		{
			if (isValid() && state_ == TransactionState::ACTIVE)
			{
				try
				{
					rollback();
				}
				catch (...)
				{
					// swallow
				}
			}
		}

	public:
		bool isValid() const noexcept { return handle_ != 0; }

		isc_tr_handle getHandle() const noexcept { return handle_; }

		isc_tr_handle* getHandlePtr() noexcept { return &handle_; }

		Attachment& getAttachment() noexcept { return attachment_; }

		TransactionState getState() const noexcept { return state_; }

		void commit();
		void commitRetaining();
		void rollback();
		void rollbackRetaining();

	private:
		Attachment& attachment_;
		isc_tr_handle handle_ = 0;
		TransactionState state_ = TransactionState::ACTIVE;
	};
}  // namespace fbcpp


#endif  // FBCPP_TRANSACTION_H
