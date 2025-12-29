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

#include "Transaction.h"
#include "Attachment.h"
#include "Client_legacy.h"
#include "Exception_legacy.h"
#include <cassert>

using namespace fbcpp;


static TPB buildTpb(const TransactionOptions& options)
{
	TPB tpb;

	if (const auto accessMode = options.getAccessMode())
	{
		switch (accessMode.value())
		{
			case TransactionAccessMode::READ_ONLY:
				tpb.addTag(isc_tpb_read);
				break;
			case TransactionAccessMode::READ_WRITE:
				tpb.addTag(isc_tpb_write);
				break;
		}
	}

	if (const auto waitMode = options.getWaitMode())
	{
		switch (waitMode.value())
		{
			case TransactionWaitMode::NO_WAIT:
				tpb.addTag(isc_tpb_nowait);
				break;
			case TransactionWaitMode::WAIT:
				tpb.addTag(isc_tpb_wait);
				break;
		}
	}

	if (const auto isolationLevel = options.getIsolationLevel())
	{
		switch (isolationLevel.value())
		{
			case TransactionIsolationLevel::CONSISTENCY:
				tpb.addTag(isc_tpb_consistency);
				break;

			case TransactionIsolationLevel::SNAPSHOT:
				tpb.addTag(isc_tpb_concurrency);
				break;

			case TransactionIsolationLevel::READ_COMMITTED:
				tpb.addTag(isc_tpb_read_committed);

				if (const auto readCommittedMode = options.getReadCommittedMode())
				{
					switch (readCommittedMode.value())
					{
						case TransactionReadCommittedMode::NO_RECORD_VERSION:
							tpb.addTag(isc_tpb_no_rec_version);
							break;
						case TransactionReadCommittedMode::RECORD_VERSION:
							tpb.addTag(isc_tpb_rec_version);
							break;
					}
				}
				break;
		}
	}

	if (options.getNoAutoUndo())
		tpb.addTag(isc_tpb_no_auto_undo);

	if (options.getIgnoreLimbo())
		tpb.addTag(isc_tpb_ignore_limbo);

	if (options.getRestartRequests())
		tpb.addTag(isc_tpb_restart_requests);

	if (options.getAutoCommit())
		tpb.addTag(isc_tpb_autocommit);

	return tpb;
}


Transaction::Transaction(Attachment& attachment, const TransactionOptions& options)
	: client{attachment.getClient()}
{
	assert(attachment.isValid());

	TPB tpb = buildTpb(options);

	StatusVector status{};

	isc_start_transaction(
		status.data(),
		&handle,
		1,  // count of databases
		attachment.getHandlePtr(),
		tpb.size(),
		const_cast<char*>(tpb.data()));

	if (hasError(status))
		throw Exception(status, "Failed to start transaction");
}

void Transaction::commit()
{
	assert(isValid());
	assert(state == TransactionState::ACTIVE || state == TransactionState::PREPARED);

	StatusVector status{};

	isc_commit_transaction(status.data(), &handle);

	handle = 0;
	state = TransactionState::COMMITTED;

	if (hasError(status))
		throw Exception(status, "Failed to commit transaction");
}

void Transaction::commitRetaining()
{
	assert(isValid());
	assert(state == TransactionState::ACTIVE);

	StatusVector status{};

	isc_commit_retaining(status.data(), &handle);

	if (hasError(status))
		throw Exception(status, "Failed to commit retaining transaction");
}

void Transaction::rollback()
{
	assert(isValid());
	assert(state == TransactionState::ACTIVE || state == TransactionState::PREPARED);

	StatusVector status{};

	isc_rollback_transaction(status.data(), &handle);

	handle = 0;
	state = TransactionState::ROLLED_BACK;

	if (hasError(status))
		throw Exception(status, "Failed to rollback transaction");
}

void Transaction::rollbackRetaining()
{
	assert(isValid());
	assert(state == TransactionState::ACTIVE);

	StatusVector status{};

	isc_rollback_retaining(status.data(), &handle);

	if (hasError(status))
		throw Exception(status, "Failed to rollback retaining transaction");
}
