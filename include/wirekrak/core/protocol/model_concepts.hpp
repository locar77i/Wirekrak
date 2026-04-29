#pragma once

#include <string_view>
#include <concepts>

#include "wirekrak/core/protocol/message_result.hpp"


namespace wirekrak::core::protocol {


// ============================================================================
// SubscriptionModel Concept
// ============================================================================
//
// A SubscriptionModel defines the protocol-specific subscription domain.
//
// It must provide:
//
//   • types
//       A compile-time list of all subscription domains
//       (used by SubscriptionController and ReplayDatabase)
//
//   • subscription_type_t<T>
//       A mapping from protocol message types (requests / acks)
//       to their canonical subscription domain
//
// NOTE:
//
//   This concept intentionally validates ONLY structural requirements.
//   It does NOT attempt to verify that subscription_type_t<T> is valid
//   for all possible T.
//
//   Correctness is enforced at instantiation sites (e.g. Session),
//   where missing mappings will produce compile-time errors.
//
// ============================================================================

template<class M>
concept SubscriptionModelConcept =
requires {
    typename M::types;
};



// ============================================================================
// MessageHandler Concept
// ============================================================================
//
// A protocol MessageHandler encapsulates protocol-specific parsing and routing logic.
//
// It must:
//   • Accept a Session Context
//   • Process a raw message (string_view)
//   • Return a MessageResult describing the outcome
//   • Be noexcept (critical for event-loop safety)
//
// It is responsible for:
//   • Parsing incoming messages
//   • Routing them to the appropriate domain
//   • Emitting events via the provided Context
//
// It must NOT:
//   • Access Session internals directly
//   • Perform blocking operations
//   • Own protocol state (subscriptions, replay, etc.)
//
// Semantics:
//   • MessageResult::Delivered    → message parsed and emitted successfully
//   • MessageResult::Backpressure → emission failed due to downstream pressure
//   • MessageResult::Invalid*     → parsing/schema/value error
//   • MessageResult::Ignored      → message not relevant for this handler
//
// ============================================================================

template<class MessageHandler, class Context>
concept MessageHandlerConcept =
requires(MessageHandler h, Context& ctx, std::string_view msg) {

    { h.on_message(ctx, msg) } noexcept -> std::same_as<MessageResult>;
};


// ============================================================================
// ProtocolModelConcept
// ============================================================================
//
// A ProtocolModel defines the full protocol contract required by Session.
//
// It must provide:
//
//   • subscription_model
//       Defines subscription domains and mappings
//
//   • messages
//       meta::type_list of all data-plane message types
//
//   • states
//       meta::type_list of all state-plane types
//
//   • message_handler
//       Stateless parser/router that operates on Session::Context
//
// This concept validates STRUCTURE only.
// Behavioral correctness is enforced at usage sites.
//
// ============================================================================

template<class M, class Context>
concept ProtocolModelConcept =
requires {
    // Required associated types
    typename M::subscription_model;
    typename M::messages;
    typename M::states;
    typename M::message_handler;
}
&& SubscriptionModelConcept<typename M::subscription_model>
&& MessageHandlerConcept<typename M::message_handler, Context>;



// ============================================================================
// PingableProtocolConcept
// ============================================================================
//
// Detects whether a ProtocolModel provides a ping() control request factory
// with a REQUIRED req_id argument.
//
// A PingableProtocolConcept must expose:
//
//   • static ping(ctrl::req_id_t)
//       Returns a control-plane request (request::Control)
//
// -----------------------------------------------------------------------------
// Rationale
// -----------------------------------------------------------------------------
//
// • Enforces a uniform control-plane contract across protocols
// • Guarantees that ping requests integrate with Session req_id tracking
// • Avoids ambiguity between overloads or optional signatures
// • Keeps Session logic simple and deterministic
//
// -----------------------------------------------------------------------------
// Usage
// -----------------------------------------------------------------------------
//
// if constexpr (PingableProtocolConcept<ProtocolModel>) {
//     session.send(ProtocolModel::ping(ctrl::PING_ID));
// }
//
// -----------------------------------------------------------------------------
// Notes
// -----------------------------------------------------------------------------
//
// • This is an OPTIONAL capability (not all protocols must implement it)
// • Only the existence and return type are validated
// • The returned type must satisfy request::Control
// • No runtime overhead (pure compile-time detection)
//
// ============================================================================

#include "wirekrak/core/protocol/request/concepts.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"

template<class M>
concept PingableProtocolConcept =
requires {
    { M::ping(ctrl::req_id_t{}) } -> request::Control;
};

} // namespace wirekrak::core::protocol
