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

#include "Attachment.h"
#include "Client_legacy.h"
#include "Exception_legacy.h"
#include <cassert>
#include <format>

using namespace fbcpp;


Attachment::Attachment(Client& client, const std::string& uri, const AttachmentOptions& options)
	: client{client},
	  uri_{uri}
{
	// Build Database Parameter Block
	DPB dpb;

	if (const auto& connectionCharSet = options.getConnectionCharSet())
		dpb.addString(isc_dpb_lc_ctype, *connectionCharSet);

	if (const auto& userName = options.getUserName())
		dpb.addString(isc_dpb_user_name, *userName);

	if (const auto& password = options.getPassword())
		dpb.addString(isc_dpb_password, *password);

	if (const auto& role = options.getRole())
		dpb.addString(isc_dpb_sql_role_name, *role);

	StatusVector status{};

	if (options.getCreateDatabase())
	{
		isc_create_database(
			status.data(),
			static_cast<short>(uri.size()),
			const_cast<char*>(uri.c_str()),
			&handle,
			dpb.size(),
			const_cast<char*>(dpb.data()),
			0);
	}
	else
	{
		isc_attach_database(
			status.data(),
			static_cast<short>(uri.size()),
			const_cast<char*>(uri.c_str()),
			&handle,
			dpb.size(),
			const_cast<char*>(dpb.data()));
	}

	if (hasError(status))
	{
		const char* operation = options.getCreateDatabase() ? "create" : "attach to";
		throw Exception(status, std::format("Attachment::{}", operation), uri);
	}
}

void Attachment::disconnectOrDrop(bool drop)
{
	assert(isValid());

	StatusVector status{};

	if (drop)
		isc_drop_database(status.data(), &handle);
	else
		isc_detach_database(status.data(), &handle);

	handle = 0;

	if (hasError(status))
	{
		const char* operation = drop ? "drop" : "detach";
		throw Exception(status, std::format("Attachment::{}", operation), uri_);
	}
}


void Attachment::disconnect()
{
	disconnectOrDrop(false);
}

void Attachment::dropDatabase()
{
	disconnectOrDrop(true);
}
