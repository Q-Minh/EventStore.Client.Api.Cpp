#pragma once

#ifndef ES_METADATA_HPP
#define ES_METADATA_HPP

namespace es {
namespace system {

struct metadata
{
	inline const static char max_age[] = "$maxAge";
	inline const static char max_count[] = "$maxCount";
	inline const static char truncate_before[] = "$tb";
	inline const static char cache_control[] = "$cacheControl";
	inline const static char acl[] = "$acl";
	inline const static char acl_read[] = "$r";
	inline const static char acl_write[] = "$w";
	inline const static char acl_delete[] = "$d";
	inline const static char acl_meta_read[] = "$mr";
	inline const static char acl_meta_write[] = "$mw";
	inline const static char user_stream_acl[] = "$userStreamAcl";
	inline const static char system_stream_acl[] = "$systemStreamAcl";
};

}
}

#endif // ES_METADATA_HPP