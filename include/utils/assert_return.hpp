/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <cassert>

#define ASSERT_RETURN_VOID( condition) /* NOLINT( cppcoreguidelines-macro-usage )*/      \
  do {                                                                                   \
    if ( ! ( condition ) )             /* NOLINT( readability-simplify-boolean-expr ) */ \
    {                                                                                    \
      assert( false );                                                                   \
      return;                                                                            \
    }                                                                                    \
  } while ( false )

#define ASSERT_RETURN( condition, value ) /* NOLINT( cppcoreguidelines-macro-usage )*/      \
  do {                                                                                      \
    if ( ! ( condition ) )                /* NOLINT( readability-simplify-boolean-expr ) */ \
    {                                                                                       \
      assert( false );                                                                      \
      return ( value );                                                                     \
    }                                                                                       \
  } while ( false )
