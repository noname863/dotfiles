return {
        "sontungexpt/stcursorword",
        event = "VeryLazy",
        config = function()
            require("stcursorword").setup({
                highlight = {
                    underline = false,
                    fg = nil,
                    bg = '#484848'
                }
            })
        end,
}

