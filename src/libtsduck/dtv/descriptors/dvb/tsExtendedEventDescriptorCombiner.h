//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2026, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------
//!
//!  @file
//!  Decode and combine the extended_event_descriptor instances of a descriptor list into whole
//!  events, even when the text is split across descriptors.
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsExtendedEventDescriptor.h"
#include "tsDescriptorList.h"

namespace ts {
    //!
    //! Decode and combine the extended_event_descriptor instances of a descriptor list into whole
    //! events, even when the text is split across descriptors.
    //!
    //! Usually, decoding each extended_event_descriptor individually produces a correct string.
    //! However, some broadcasters first encode the full text and then slice the resulting binary
    //! blob at an arbitrary point to fit each descriptor.
    //! A descriptor can then end in the middle of the encoded text and no longer be valid as
    //! a string on its own.
    //! This has been observed with ARIB STD-B24 strings in ISDB streams from NHK.
    //!
    //! @param [in,out] duck TSDuck execution context.
    //! @param [in] dlist Descriptor list to read.
    //! @return Reassembled descriptors in descriptor-list order.
    //! @ingroup libtsduck descriptor
    //!
    TSDUCKDLL std::list<ExtendedEventDescriptor> CombineExtendedEventDescriptors(DuckContext& duck, const DescriptorList& dlist);
}
