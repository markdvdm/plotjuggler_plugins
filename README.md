# plotjuggler_plugins
Plotjuggler plugins for Elroy Air

# Setup instructions
To use plotjuggler plugins, you will need to install plotjuggler from source. The snap install does not work for this and the version packaged with ros2 is out of date.

see https://github.com/facontidavide/PlotJuggler/blob/main/COMPILE.md for instructions.
After installing plotjuggler, you can run it like this: `~/plotjuggler_ws/install/lib/plotjuggler/plotjuggler -n`

In the top left, select "App->preferences"
Then select "plugins" and add `/home/<user>/code/plotjuggler_plugins/build`

In plotjuggler_plugins, update the git submodules and build
`./scripts/build.sh`
