using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Lumix
{
	[NativeComponent(Type = "hinge_joint")]
	public class HingeJoint : Component
	{
		public HingeJoint(Entity _entity)
			: base(_entity,  getModule(_entity.instance_, "hinge_joint" )) { }


		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static Vec3 getAxisPosition(IntPtr module, int cmp);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static void setAxisPosition(IntPtr module, int cmp, Vec3 value);


		public Vec3 AxisPosition
		{
			get { return getAxisPosition(module_, entity_.entity_Id_); }
			set { setAxisPosition(module_, entity_.entity_Id_, value); }
		}

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static Vec3 getAxisDirection(IntPtr module, int cmp);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static void setAxisDirection(IntPtr module, int cmp, Vec3 value);


		public Vec3 AxisDirection
		{
			get { return getAxisDirection(module_, entity_.entity_Id_); }
			set { setAxisDirection(module_, entity_.entity_Id_, value); }
		}

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static float getDamping(IntPtr module, int cmp);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static void setDamping(IntPtr module, int cmp, float value);


		public float Damping
		{
			get { return getDamping(module_, entity_.entity_Id_); }
			set { setDamping(module_, entity_.entity_Id_, value); }
		}

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static float getStiffness(IntPtr module, int cmp);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static void setStiffness(IntPtr module, int cmp, float value);


		public float Stiffness
		{
			get { return getStiffness(module_, entity_.entity_Id_); }
			set { setStiffness(module_, entity_.entity_Id_, value); }
		}

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static bool getUseLimit(IntPtr module, int cmp);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static void setUseLimit(IntPtr module, int cmp, bool value);


		public bool IsUseLimit
		{
			get { return getUseLimit(module_, entity_.entity_Id_); }
			set { setUseLimit(module_, entity_.entity_Id_, value); }
		}

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static Vec2 getLimit(IntPtr module, int cmp);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern static void setLimit(IntPtr module, int cmp, Vec2 value);


		public Vec2 Limit
		{
			get { return getLimit(module_, entity_.entity_Id_); }
			set { setLimit(module_, entity_.entity_Id_, value); }
		}

	} // class
} // namespace
