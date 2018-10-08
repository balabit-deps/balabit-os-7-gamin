

#ifndef __MD_EVENT_H__
#define __MD_EVENT_H__

typedef enum {
	GAMIN_EVENT_CHANGED = 1 << 4,
	GAMIN_EVENT_CREATED = 1 << 5,
	GAMIN_EVENT_DELETED = 1 << 6,
	GAMIN_EVENT_MOVED = 1 << 7,
	GAMIN_EVENT_EXISTS = 1 << 8,
	GAMIN_EVENT_ENDEXISTS = 1 << 9,
	GAMIN_EVENT_UNKNOWN = 1 << 10
} GaminEventType;

const char *gam_event_to_string (GaminEventType event);

#endif /* __MD_EVENT_H__ */
