/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <gtest/gtest.h>

#include "smtp.hpp"

using namespace curlev;

// SMTP can be tested using:
//   docker run --rm -it -p 3000:80 -p 2525:25 rnwood/smtp4dev:v3

//--------------------------------------------------------------------
TEST( smtp, address )
{
  smtp::address a;
  //
  a = smtp::address( R"( Joe Q. Public <john.q.public@example.com> )" );
  EXPECT_EQ( a.address_spec, "john.q.public@example.com" );
  EXPECT_EQ( a.display_name, "Joe Q. Public"   );
  EXPECT_EQ( a.get_name_addr(), R"("Joe Q. Public" <john.q.public@example.com>)" );
  EXPECT_EQ( a.get_addr_spec(),  R"(<john.q.public@example.com>)" );
  //
  a = smtp::address( R"( Mary Smith <mary@x.test>)" );
  EXPECT_EQ( a.address_spec, "mary@x.test"        );
  EXPECT_EQ( a.display_name, "Mary Smith"     );
  EXPECT_EQ( a.get_name_addr(), R"("Mary Smith" <mary@x.test>)" );
  EXPECT_EQ( a.get_addr_spec(),  R"(<mary@x.test>)" );
  //
  a = smtp::address( R"(jdoe@example.org )" );
  EXPECT_EQ( a.address_spec, "jdoe@example.org"     );
  EXPECT_EQ( a.display_name, ""          );
  EXPECT_EQ( a.get_name_addr(), R"(<jdoe@example.org>)" );
  EXPECT_EQ( a.get_addr_spec(),  R"(<jdoe@example.org>)" );
  //
  a = smtp::address( R"(Who? <one@y.test>)" );
  EXPECT_EQ( a.address_spec, "one@y.test"        );
  EXPECT_EQ( a.display_name, "Who?"        );
  EXPECT_EQ( a.get_name_addr(), R"("Who?" <one@y.test>)" );
  EXPECT_EQ( a.get_addr_spec(),  R"(<one@y.test>)" );
  //
  a = smtp::address( R"( <boss@nil.test>)" );
  EXPECT_EQ( a.address_spec, "boss@nil.test"       );
  EXPECT_EQ( a.display_name, ""          );
  EXPECT_EQ( a.get_name_addr(), R"(<boss@nil.test>)" );
  EXPECT_EQ( a.get_addr_spec(),  R"(<boss@nil.test>)" );
  //
  a = smtp::address( R"(Big" Box <sysservices@example.net> )" );
  EXPECT_EQ( a.address_spec, "sysservices@example.net"  );
  EXPECT_EQ( a.display_name, "Big\" Box"     );
  EXPECT_EQ( a.get_name_addr(), R"("Big" Box" <sysservices@example.net>)" );
  EXPECT_EQ( a.get_addr_spec(),  R"(<sysservices@example.net>)" );
  //
  a = smtp::address( R"( "Joe Q. Public" <john.q.public@example.com> )" );
  EXPECT_EQ( a.address_spec, "john.q.public@example.com" );
  EXPECT_EQ( a.display_name, "Joe Q. Public"   );
  EXPECT_EQ( a.get_name_addr(), R"("Joe Q. Public" <john.q.public@example.com>)" );
  EXPECT_EQ( a.get_addr_spec(),  R"(<john.q.public@example.com>)" );
  //
  a = R"( "Giant; "Big" Box" <sysservices@example.net> )";
  EXPECT_EQ( a.address_spec, "sysservices@example.net"  );
  EXPECT_EQ( a.display_name, "Giant; \"Big\" Box" );
  EXPECT_EQ( a.get_name_addr(), R"("Giant; "Big" Box" <sysservices@example.net>)" );
  EXPECT_EQ( a.get_addr_spec(),  R"(<sysservices@example.net>)" );
}

//--------------------------------------------------------------------
TEST( smtp, send )
{
  ASync async;
  async.start();
  //
  {
    auto smtp = SMTP::create( async );
    //
    auto future =
      smtp->SEND(
        "smtp://localhost:2525",
        smtp::address( "sender@example.com" ),
        { smtp::address( "address@example.com" ) },
        "Date: Mon, 29 Nov 2010 21:54:29 +1100\r\n"
        "To: address@example.com\r\n"
        "From: sender@example.com\r\n"
        "Subject: SMTP example message\r\n"
        "\r\n"
        "The body of the message.\r\n" )
      .launch();
    //
    auto response = future.get();
    //
    EXPECT_EQ( response.code, 7 );
  }
  //
  {
    auto smtp = SMTP::create( async );
    //
    auto code =
      smtp->SEND( "smtp://localhost:2525",
                  smtp::address( "sender@example.com" ),
                  { smtp::address( "Joe Q. Public <john.q.public@example.com> )" ),
                    smtp::address( "<boss@nil.test>" , smtp::address::Mode::cc  ),
                    smtp::address( "archive@test.com", smtp::address::Mode::bcc ) },
                  "Test Subject",
                  { mime::alternatives{
                      mime::data{ "", "Hello"       , "text/plain", "" },
                      mime::data{ "", "<b>Hello</b>", "text/html" , "" }
                    },
                    mime::data{ "a.txt", "abc123", "text/plain" , "a.txt"  }
                  } )
      .add_headers( { { "Priority", "urgent" } } )
      .exec()
      .get_code();
    //
    EXPECT_EQ( code, 7 );
  }
}
