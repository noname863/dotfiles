return {
    'nvim-telescope/telescope.nvim', tag = '0.1.8',

    dependencies = { 'nvim-lua/plenary.nvim' },

    config = function()
        require("telescope").setup({})

        local builtin = require('telescope.builtin')
        vim.keymap.set('n', '<leader>pf', builtin.find_files, { desc = 'Telescope find files' })
        vim.keymap.set('n', '<C-p>', builtin.git_files, { desc = 'Telescope find files in project' })
        vim.keymap.set('n', '<leader>ps',
            function() builtin.grep_string({ search = vim.fn.input("Grep > ") }); end,
            { desc = 'Telescope search files' })
        vim.keymap.set('n', '<C-s>', builtin.live_grep, { desc = 'Telescope search files in project' })
        vim.keymap.set('n', '<C-g>',
            function()
                builtin.lsp_references({ show_line = false })
            end,
            { desc = 'Telescope references' })
        vim.keymap.set('n', '<C-t>',
            function()
                builtin.lsp_dynamic_workspace_symbols(
                {
                    fname_width = 0,
                    ignore_symbols = "field"
                })
            end,
            { desc = 'Telescope search in references'} );
    end
}

