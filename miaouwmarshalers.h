
#ifndef __miaouw_marshal_MARSHAL_H__
#define __miaouw_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* BOOLEAN:BOXED (miaouwmarshalers.list:1) */
extern void miaouw_marshal_BOOLEAN__BOXED (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);

/* VOID:OBJECT,OBJECT (miaouwmarshalers.list:2) */
extern void miaouw_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);

G_END_DECLS

#endif /* __miaouw_marshal_MARSHAL_H__ */

