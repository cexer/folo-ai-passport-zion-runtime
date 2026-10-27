"""Admission rules for the user's three consecutive unchanged-base cases."""
def qualifies(result,source_unchanged,wrappers,base_changed):
    return bool(result.get('mainflow_pass') is True and
                result.get('environment')=='physical device' and
                result.get('automated_flow_pass') is True and
                result.get('visual_review') and source_unchanged and
                not wrappers and not base_changed)

def advance(streak,clean):
    return streak+1 if clean else 0
