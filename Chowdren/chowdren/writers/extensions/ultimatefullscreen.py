from chowdren.writers.objects import ObjectWriter

from chowdren.common import get_animation_name, to_c, make_color

from chowdren.writers.events import (ActionMethodWriter, ConditionMethodWriter,
    ExpressionMethodWriter, make_table, ActionWriter)

class UltimateFullcsreen(ObjectWriter):
    class_name = 'Fullscreen'
    static = True

class SetFullscreenType(ActionMethodWriter):
    custom = True

    def write(self, writer):
        parameter = self.convert_index(0)
        # if parameter != '0':
        #     print('Fullscreen type not supported:', parameter)
        #     raise NotImplementedError()

actions = make_table(ActionMethodWriter, {
    1 : 'manager.set_window(false)',
    0 : 'manager.set_window(true)',
    5 : 'manager.set_window_scale',
    4 : SetFullscreenType
})

conditions = make_table(ConditionMethodWriter, {
    1 : '!manager.is_fullscreen()'
})

expressions = make_table(ExpressionMethodWriter, {
    0 : 'WindowControl::get_x',
    1 : 'WindowControl::get_y'
})

def get_object():
    return UltimateFullcsreen