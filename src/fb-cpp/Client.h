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

#ifndef FBCPP_CLIENT_H
#define FBCPP_CLIENT_H

#include "config.h"
#include "fb-api.h"
#include <cassert>
#include <memory>
#include <string>

#if FB_CPP_USE_BOOST_DLL != 0
#include <boost/dll.hpp>
#endif


///
/// fb-cpp namespace.
///
namespace fbcpp
{
	///
	/// Represents a Firebird client library instance.
	///
	/// With the Firebird 2.5 C API, the Client class primarily serves as:
	/// - A library loader when using Boost.DLL to dynamically load fbclient
	/// - A marker/factory for creating connections
	///
	/// The Client must exist and remain valid while there are other objects using it,
	/// such as Attachment, Transaction and Statement.
	///
	class Client final
	{
	public:
		///
		/// Constructs a Client object that uses the system-installed fbclient library.
		///
		Client() = default;

#if FB_CPP_USE_BOOST_DLL != 0
		///
		/// Constructs a Client object that loads the Firebird client library (or embedded engine)
		/// from the specified path using Boost.DLL.
		///
		explicit Client(const boost::dll::fs::path& fbclientLibPath,
			boost::dll::load_mode::type loadMode = boost::dll::load_mode::append_decorations |
				boost::dll::load_mode::search_system_folders)
			: fbclientLib_{fbclientLibPath, loadMode},
			  usingDynamicLib_{true}
		{
		}

		///
		/// Constructs a Client object that uses the specified Boost.DLL shared_library
		/// representing the Firebird client library (or embedded engine).
		///
		explicit Client(boost::dll::shared_library fbclientLib)
			: fbclientLib_{std::move(fbclientLib)},
			  usingDynamicLib_{true}
		{
		}
#endif

		///
		/// Move constructor.
		/// A moved Client object becomes invalid.
		///
		Client(Client&& o) noexcept
			: valid_{o.valid_}
#if FB_CPP_USE_BOOST_DLL != 0
			  ,
			  fbclientLib_{std::move(o.fbclientLib_)},
			  usingDynamicLib_{o.usingDynamicLib_}
#endif
		{
			o.valid_ = false;
		}

		///
		/// If the Client object was created using Boost.DLL, this destructor just releases
		/// the internal boost::dll::shared_library resource.
		///
		~Client() noexcept = default;

		Client& operator=(Client&&) = delete;
		Client& operator=(const Client& o) = delete;
		Client(const Client&) = delete;

	public:
		///
		/// Returns whether the Client object is valid.
		///
		bool isValid() const noexcept
		{
			return valid_;
		}

#if FB_CPP_USE_BOOST_DLL != 0
		///
		/// Returns whether using a dynamically loaded library.
		///
		bool isDynamicLibrary() const noexcept
		{
			return usingDynamicLib_;
		}

		///
		/// Returns the loaded library (if using Boost.DLL).
		///
		const boost::dll::shared_library& getLibrary() const noexcept
		{
			return fbclientLib_;
		}
#endif

	private:
		bool valid_ = true;
#if FB_CPP_USE_BOOST_DLL != 0
		boost::dll::shared_library fbclientLib_;
		bool usingDynamicLib_ = false;
#endif
	};
}  // namespace fbcpp


#endif  // FBCPP_CLIENT_H
