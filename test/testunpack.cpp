/*
 * Copyright (C) 2008 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


#include <boost/test/auto_unit_test.hpp>

#include "unpack.h"

using boost::unit_test::test_suite;


void test_unpack()
{
	const uint8_t packed[] = {0x12, 0x34, 0x56, 0x78, 0x90, 0xAB };
	uint8_t unpacked[8];

	size_t s = OpenRaw::Internals::unpack_12to16(unpacked, 8, packed, 6);
	BOOST_CHECK_EQUAL(s, 8);
	BOOST_CHECK_EQUAL(unpacked[0], 0x01);
	BOOST_CHECK_EQUAL(unpacked[1], 0x23);
	BOOST_CHECK_EQUAL(unpacked[2], 0x04);
	BOOST_CHECK_EQUAL(unpacked[3], 0x56);
	BOOST_CHECK_EQUAL(unpacked[4], 0x07);
	BOOST_CHECK_EQUAL(unpacked[5], 0x89);
	BOOST_CHECK_EQUAL(unpacked[6], 0x00);
	BOOST_CHECK_EQUAL(unpacked[7], 0xAB);
}


test_suite*
init_unit_test_suite( int /*argc*/, char ** /*argv*/ ) 
{
	test_suite* test = BOOST_TEST_SUITE("test unpack");
	
	test->add(BOOST_TEST_CASE(&test_unpack));

	return test;
}
