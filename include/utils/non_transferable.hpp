/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

namespace curlev
{

// To make a class non-copyable and non-movable, inherit from this class
class non_transferable
{
public:
  virtual ~non_transferable()                              = default;
  //
  non_transferable            ( const non_transferable & ) = delete;
  non_transferable & operator=( const non_transferable & ) = delete;
  non_transferable            ( non_transferable && )      = delete;
  non_transferable & operator=( non_transferable && )      = delete;
  //
protected:
  non_transferable()                                       = default;
};

} // namespace curlev
