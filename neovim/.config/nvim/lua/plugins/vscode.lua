return {
    'Mofiqul/vscode.nvim',

    config = function()
        local c = require("vscode.colors").get_colors()

        require("vscode").setup({
            transparent = true,
            italic_comments = true,
            underline_links = true,

            group_overrides = {
                Structure = { link = 'StorageClass' },
                DiagnosticUnderlineWarn =
                    {
                        fg = 'NONE',
                        bg = 'NONE',
                        undercurl = true,
                        sp = c.vscYellowOrange
                    },
                DiagnosticWarn =
                    {
                        fg = c.vscYellowOrange,
                        bg = "None"
                    },
            },
        })

        vim.cmd.colorscheme "vscode"
    end
}

