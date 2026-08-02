## How to run : 
```bash
xhost +local:docker

docker run --rm \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  --device /dev/dri \
  -v $(pwd)/imgui.ini:/app/imgui.ini \
  -w /app \
  imgui-sdl3-opengl3
```