# C# plugin for [Lumix Engine](https://github.com/nem0/lumixengine)
-------
**Warning: the latest version is not in usable state!**

Getting started:
1. Install [mono](https://www.mono-project.com/download/stable/)
2. [Install this plugin](https://github.com/nem0/LumixEngine/wiki/available-plugins)
3. Build and run Lumix Engine Studio, you can now create and use C# scripts

## Videos 
* [See this plugin in action](https://www.youtube.com/watch?v=jZrPzzhROqc)
* [Debugging](https://www.youtube.com/watch?v=4U7PQ3zR6Ok)

## Sample

```csharp
public class Test : Lumix.Component
{
	float yaw = 0;
	void OnInput(Lumix.InputEvent e)
	{
		if (e is Lumix.MouseAxisInputEvent)
		{
			var ev = e as Lumix.MouseAxisInputEvent;
			yaw += ev.x * 0.02f;
		}
	}

	void Update(float td)
	{
		this.entity.Rotation = Lumix.Quat.FromAngleAxis(yaw, new Lumix.Vec3(0, 1, 0));
	}
}

```
