#pragma once

#ifndef ES_GET_STREAM_METADATA_HPP
#define ES_GET_STREAM_METADATA_HPP

#include "stream_metadata_result_raw.hpp"
#include "read_stream_event.hpp"
#include "system/streams.hpp"

namespace es {

template <class ConnectionType, class StreamMetadataReadResultHandler>
void async_get_stream_metadata(
	std::shared_ptr<ConnectionType> const& connection,
	std::string_view stream,
	StreamMetadataReadResultHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<StreamMetadataReadResultHandler, std::error_code, std::optional<stream_metadata_result_raw>>,
		"StreamMetadataReadResultHandler requirements not met, must have signature R(std::error_code, std::optional<es::stream_metadata_result_raw>)"
	);

	async_read_stream_event(
		connection,
		system::streams::meta_stream_of(stream),
		std::int64_t(-1),
		false,
		[handler = std::move(handler), stream = stream](std::error_code ec, std::optional<event_read_result> result)
	{
		if (!ec)
		{
			auto& value = result.value();
			switch (value.status())
			{
			case event_read_status::success:
			{
				if (!value.event().has_value())
				{
					ec = make_error_code(communication_errors::unexpected_response);
				}
				else
				{
					if (!value.event().value().original_event().has_value())
					{
						auto metadata_result = std::make_optional(stream_metadata_result_raw{ 
							std::string(stream), false, std::int64_t(-1), std::string("")
							});
						handler(ec, std::move(metadata_result));
						return;
					}
					else
					{
						auto& event = value.event().value().original_event().value();

						std::string content;
						using std::swap;
						swap(const_cast<std::string&>(event.content()), content);

						auto metadata_result = std::make_optional(stream_metadata_result_raw{ 
							std::string(stream), false, event.event_number(), std::move(content)
							});
						handler(ec, std::move(metadata_result));
						return;
					}
				}
				handler(ec, {});
				return;
			}

			case event_read_status::not_found:
			case event_read_status::no_stream:
			{
				auto metadata_result = std::make_optional(stream_metadata_result_raw{
					std::string(stream), false, std::int64_t(-1), std::string("")
					});
				handler(ec, std::move(metadata_result));
				return;
			}
			case event_read_status::stream_deleted:
			{
				auto metadata_result = std::make_optional(stream_metadata_result_raw{
					std::string(stream), true, std::numeric_limits<std::int64_t>::max(), std::string("")
					});
				handler(ec, std::move(metadata_result));
				break;
			}
			default:
				// should never happen anyways, it's just for compiler warnings
				ec = make_error_code(communication_errors::unexpected_response);
				break;
			}
			return;
		}
		else
		{
			handler(ec, {});
			return;
		}
	}
	);
}

}

#endif // ES_GET_STREAM_METADATA_HPP