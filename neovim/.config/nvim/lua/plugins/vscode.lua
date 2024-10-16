return {
    'Mofiqul/vscode.nvim',

    config = function()
        local c = require("vscode.colors").get_colors()
        
        require("vscode").setup({
            transparent = true,
            italic_comments = true,
            underline_links = true,
        })

        vim.cmd.colorscheme "vscode"
    end
}

