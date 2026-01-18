#pragma once

/*
===============================================================================
Wirekrak — Public API Entry Point
===============================================================================

This is the primary public entry point for Wirekrak.

Wirekrak exposes a stable, user-facing API via Wirekrak Lite, built on top
of Wirekrak Core. Lite is designed for application developers and data
consumers who want safe defaults and a clean integration surface.

Only symbols declared in the wirekrak::lite namespace are part of the
public API contract.
===============================================================================
*/

#include <wirekrak/lite.hpp>
