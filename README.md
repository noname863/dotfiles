
Always work in progress dotfiles for my system. Most of them deployable by
[stow](https://brandon.invergo.net/news/2012-05-26-using-gnu-stow-to-manage-your-dotfiles.html),
some require build though.

Basically, all folders except for src correspond to "stow package", and, given
this repo is cloned in your home directory, can be deployed with 
`stow -S "directory"`. However some configs, (for now waybar) require build of
binaries. Code is in src folder, and there is a script, which will build 
required programs, install them into repo\_root/wayland/.local/ which will
install them in your home/username/.local after you call stow -S wayland.

build will require python3 and modern clang toolchain (at the moment of writing
only clang can compile project)

