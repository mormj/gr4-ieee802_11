/*
 * Copyright (C) 2016 Bastian Bloessl <bloessl@ccs-labs.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gnuradio/kernel/digital/constellation.h>

using constellation = gr::kernel::digital::constellation;

namespace gr {
namespace ieee802_11 {

class constellation_bpsk : virtual public constellation
{
public:
	typedef std::shared_ptr<gr::ieee802_11::constellation_bpsk> sptr;
	static sptr make();
	constellation_bpsk();
	unsigned int decision_maker(const gr_complex *sample);
};

class constellation_qpsk : virtual public gr::kernel::digital::constellation
{
public:
	typedef std::shared_ptr<gr::ieee802_11::constellation_qpsk> sptr;
	static sptr make();
	constellation_qpsk();
	unsigned int decision_maker(const gr_complex *sample);
};

class constellation_16qam : virtual public gr::kernel::digital::constellation
{
public:
	typedef std::shared_ptr<gr::ieee802_11::constellation_16qam> sptr;
	static sptr make();
	constellation_16qam();
	unsigned int decision_maker(const gr_complex *sample);
};

class constellation_64qam : virtual public gr::kernel::digital::constellation
{
public:
	typedef std::shared_ptr<gr::ieee802_11::constellation_64qam> sptr;
	static sptr make();
	constellation_64qam();
	unsigned int decision_maker(const gr_complex *sample);
};

} // namespace wifi
} // namespace gr
