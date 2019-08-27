#pragma once

#ifndef ES_DURATION_CONVERSIONS_HPP
#define ES_DURATION_CONVERSIONS_HPP

#include <chrono>

#define ES_SECONDS_CAST(expr) std::chrono::duration_cast<std::chrono::seconds>(expr)
#define ES_MILLISECONDS_CAST(expr) std::chrono::duration_cast<std::chrono::milliseconds>(expr)
#define ES_NANOSECONDS_CAST(expr) std::chrono::duration_cast<std::chrono::nanoseconds>(expr)

#define ES_SECONDS(expr) ES_SECONDS_CAST(expr).count()
#define ES_MILLISECONDS(expr) ES_MILLISECONDS_CAST(expr).count()
#define ES_NANOSECONDS(expr) ES_NANOSECONDS_CAST(expr).count()

#endif // ES_DURATION_CONVERSIONS_HPP