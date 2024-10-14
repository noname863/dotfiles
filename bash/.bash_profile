# !/usr/bash

# Get the aliases and functions
if [ -f ~/.bashrc ]; then
    . ~/.bashrc
fi

# User specific environment and startup programs
pathmunge () {
    # local -n ref=$1
    case ":{${!1}}:" in
        *:"${!1}":*)
            ;;
        *)
	    prev=${!1}
            if [ "$3" = "after" ] ; then
                eval "$1=$prev:$2"
            else
                eval "$1=$2:$prev"
            fi
    esac
}

export LD_LIBRARY_PATH="$HOME/.local/lib"

pathmunge PATH "$HOME/.cargo/bin"
pathmunge PATH "$HOME/bin"
pathmunge PATH "$HOME/.local/bin"

# pathmunge LD_LIBRARY_PATH "$HOME/lib"
# pathmunge LD_LIBRARY_PATH "$HOME/.local/lib"

unset -f pathmunge

