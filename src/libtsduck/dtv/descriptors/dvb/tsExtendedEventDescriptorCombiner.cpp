//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2026, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------

#include "tsExtendedEventDescriptorCombiner.h"
#include "tsExtendedEventDescriptor.h"
#include "tsDescriptor.h"
#include "tsDescriptorList.h"
#include "tsPSIBuffer.h"
#include "tsDuckContext.h"
#include "tsByteBlock.h"
#include "tsDID.h"


namespace {
    struct RawEntry
    {
        ts::UString   item_description {};
        ts::ByteBlock item_chars {};
    };

    struct RawExtendedEvent
    {
        uint8_t              descriptor_number = 0;
        ts::UString          language_code {};
        std::list<RawEntry>  entries {};
        ts::ByteBlock        text_chars {};
    };

    bool ReadRawExtendedEvent(ts::DuckContext& duck, const ts::Descriptor& desc, RawExtendedEvent& out)
    {
        if (!desc.isValid() || desc.tag() != ts::DID_DVB_EXTENDED_EVENT) {
            return false;
        }
        ts::PSIBuffer buf(duck, desc.payload(), desc.payloadSize());

        buf.getBits(out.descriptor_number, 4);
        buf.skipBits(4); // skip last_descriptor_number: check contiguity by descriptor_number alone instead
        buf.getLanguageCode(out.language_code);
        buf.pushReadSizeFromLength(8); // start length_of_items
        while (buf.canRead()) {
            RawEntry entry;
            buf.getStringWithByteLength(entry.item_description);
            buf.getBytes(entry.item_chars, buf.getUInt8());
            out.entries.push_back(std::move(entry));
        }
        buf.popState(); // close length_of_items
        buf.getBytes(out.text_chars, buf.getUInt8());

        return !buf.error() && buf.endOfRead();
    }

    class ExtendedEventDescriptorCombiner
    {
    public:
        explicit ExtendedEventDescriptorCombiner(ts::DuckContext& duck) : _duck(duck) {}

        void add(const RawExtendedEvent& source)
        {
            const bool contiguous = _hasPending && source.language_code == _combined.language_code &&
                                    source.descriptor_number != 0 &&
                                    source.descriptor_number == _expected_descriptor_number;
            if (!contiguous) {
                flush();
                _hasPending = true;
                _combined.language_code = source.language_code;
            }

            for (const auto& entry : source.entries) {
                // An empty item_description means this entry is a continuation of the previous one
                // when there is a previous entry.
                // Append its item_chars instead of inserting a new item.
                const bool continuation = !_combined.entries.empty() && entry.item_description.empty();
                if (continuation) {
                    _item_chars.back().insert(_item_chars.back().end(), entry.item_chars.begin(), entry.item_chars.end());
                }
                else {
                    _combined.entries.push_back(ts::ExtendedEventDescriptor::Entry(entry.item_description));
                    _item_chars.push_back(entry.item_chars);
                }
            }
            _text_chars.insert(_text_chars.end(), source.text_chars.begin(), source.text_chars.end());

            _expected_descriptor_number = uint8_t((source.descriptor_number + 1) & 0x0F);
        }

        std::list<ts::ExtendedEventDescriptor> finish()
        {
            flush();
            return std::move(_result);
        }

    private:
        // Finalize the current descriptor group and append it to the result.
        void flush()
        {
            if (!_hasPending) {
                return;
            }
            auto chars = _item_chars.begin();
            for (auto& entry : _combined.entries) {
                (void)_duck.decode(entry.item, chars->data(), chars->size());
                ++chars;
            }
            (void)_duck.decode(_combined.text, _text_chars.data(), _text_chars.size());

            _result.push_back(std::move(_combined));
            _combined = ts::ExtendedEventDescriptor {};
            _item_chars.clear();
            _text_chars.clear();
            _hasPending = false;
        }

        ts::DuckContext& _duck;
        std::list<ts::ExtendedEventDescriptor> _result {};
        ts::ExtendedEventDescriptor _combined {};
        // List of raw item_char bytes, decoded into entry.item in flush().
        ts::ByteBlockList _item_chars {};
        // Raw text_char bytes, decoded into _combined.text in flush().
        ts::ByteBlock _text_chars {};
        uint8_t _expected_descriptor_number = 0;
        bool _hasPending = false;
    };
}


//----------------------------------------------------------------------------
// Combine the ExtendedEventDescriptor instances of a descriptor list.
//----------------------------------------------------------------------------

std::list<ts::ExtendedEventDescriptor> ts::CombineExtendedEventDescriptors(DuckContext& duck, const DescriptorList& dlist)
{
    ExtendedEventDescriptorCombiner combiner(duck);

    for (size_t index = dlist.search(DID_DVB_EXTENDED_EVENT); index < dlist.count();
         index = dlist.search(DID_DVB_EXTENDED_EVENT, index + 1)) {
        RawExtendedEvent source;
        if (ReadRawExtendedEvent(duck, dlist[index], source)) {
            combiner.add(source);
        }
    }
    return combiner.finish();
}
