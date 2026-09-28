//=========================================================================
// Name:            DeliveryChip.h
// Purpose:         Which delivery chip a sent chat message shows.
//
// Authors:         FreeDV text messaging contributors
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#ifndef TEXT_MESSAGING__DELIVERY_CHIP_H
#define TEXT_MESSAGING__DELIVERY_CHIP_H

#include "TextMessagingTypes.h"

namespace TextMessaging
{

// The chip on the right of a sent message in the chat window. The window owns
// the wording and the colours; which chip applies lives here, where it can be
// tested, because it has been wrong before in ways that were hard to see.
enum class DeliveryChipKind
{
    None,            // a received message has no chip
    Queued,
    Sending,         // the first attempt, on the air
    Sent,            // the first attempt, or a broadcast, done
    Retry,           // an attempt after one that got nothing through
    Resend,          // the far end has part of it; the rest is on its way
    Acknowledged,
    NotAcknowledged, // out of retries
    NotSent,         // discarded before it went out
};

struct DeliveryChipState
{
    DeliveryChipKind kind = DeliveryChipKind::None;
    int retry = 0;              // for Retry
    int fragmentsConfirmed = 0; // for a partly delivered message: how far it got
    int fragmentCount = 0;

    // Worth showing only between the first fragment confirmed and the last.
    bool showsProgress() const
    {
        return fragmentsConfirmed > 0 && fragmentsConfirmed < fragmentCount;
    }
};

inline DeliveryChipState deliveryChipState(const TextMessage& message)
{
    DeliveryChipState state;
    state.retry = message.retryCount;
    state.fragmentsConfirmed = message.fragmentsConfirmed;
    state.fragmentCount = message.fragmentCount;

    switch (message.status)
    {
        case MessageStatus::Queued:
            state.kind = DeliveryChipKind::Queued;
            break;
        case MessageStatus::Transmitting:
        case MessageStatus::AwaitingAck:
        case MessageStatus::Retrying:
            // Only the first attempt is plain SENDING or SENT. A retransmission
            // keeps its retry number through the whole attempt, on the air and
            // while the acknowledgement timer runs, or the chip appears to go
            // backwards every time the message returns to the air. A message
            // the far end holds part of shows that instead of starting over.
            if (message.retryCount > 0)
            {
                state.kind = DeliveryChipKind::Retry;
            }
            else if (message.fragmentsConfirmed > 0)
            {
                state.kind = DeliveryChipKind::Resend;
            }
            else
            {
                state.kind = message.status == MessageStatus::Transmitting
                                 ? DeliveryChipKind::Sending
                                 : DeliveryChipKind::Sent;
            }
            break;
        case MessageStatus::Acknowledged:
            state.kind = DeliveryChipKind::Acknowledged;
            break;
        case MessageStatus::Failed:
            state.kind = DeliveryChipKind::NotAcknowledged;
            break;
        case MessageStatus::NotSent:
            state.kind = DeliveryChipKind::NotSent;
            break;
        case MessageStatus::Sent:
            state.kind = DeliveryChipKind::Sent;
            break;
        case MessageStatus::Received:
            state.kind = DeliveryChipKind::None;
            break;
    }

    return state;
}

} // namespace TextMessaging

#endif // TEXT_MESSAGING__DELIVERY_CHIP_H
