return {
    "neovim/nvim-lspconfig",
    dependencies = {
        "williamboman/mason.nvim",
        "williamboman/mason-lspconfig.nvim",

        -- { "ms-jpq/coq_nvim", branch = "coq" },
        -- { "ms-jpq/coq.thirdparty", branch = "3p" }

        "hrsh7th/cmp-nvim-lsp",
        "hrsh7th/cmp-nvim-lsp-signature-help",
        "hrsh7th/nvim-cmp",
    },

    lazy = false,

    -- init = function()
    --     vim.g.coq_settings = {
    --         auto_start = "shut-up",
    --         completion = {
    --             always = true,
    --             smart = true,
    --         },
    --         keymap = {
    --             pre_select = true,
    --             recommended = false,
    --         },
    --         display = {
    --             ghost_text = {
    --                 enabled = false,
    --             },
    --         },
    --         clients = {
    --             tags = {
    --                 enabled = false,
    --             },
    --             snippets = {
    --                 enabled = false,
    --             },
    --             paths = {
    --                 enabled = false,
    --             },
    --             tree_sitter = {
    --                 enabled = false,
    --             },
    --             buffers = {
    --                 enabled = false,
    --             },
    --             registers = {
    --                 enabled = false,
    --             },
    --             tmux = {
    --                 enabled = false,
    --             },
    --             tabnine = {
    --                 enabled = false,
    --             },
    --         },
    --         limits = {
    --             completion_auto_timeout = 0
    --         }
    --     }
    -- end,

    config = function()

        require("mason").setup()

        local lspconfig = require("lspconfig")
        -- local coq = require("coq")
        local cmp = require("cmp")
        local capabilities = require("cmp_nvim_lsp").default_capabilities()

        require("mason-lspconfig").setup({
            ensure_installed = {
                "clangd",
                "lua_ls",
                "typos_lsp"
            },
            handlers = {
                function(server_name) -- default handler
                    lspconfig[server_name].setup({
                        capabilities = capabilities
                    })
                end,
                ["typos_lsp"] = function()
                    lspconfig.typos_lsp.setup({
                        init_options = {
                            diagnosticSeverity = "Error",
                        },
                    })
                end,
                ["lua_ls"] = function()
                    local settings = {
                        capabilities = capabilities,
                        settings = {
                            Lua = {
                                runtime = { version = "Lua 5.4" },
                                diagnostics = {
                                    globals = { "bit", "vim", "it", "describe",
                                        "before_each", "after_each"},
                                },
                            },
                        },
                    }
                    lspconfig.lua_ls.setup({settings})
                end,
            },
        })

        vim.api.nvim_create_autocmd('LspAttach', {
            callback = function(args)
                -- Unset 'omnifunc'
                vim.bo[args.buf].omnifunc = nil
                -- Unset 'tagfunc'
                vim.bo[args.buf].tagfunc = nil

                local client = vim.lsp.get_client_by_id(args.data.client_id)
                if client.supports_method('textDocument/implementation') then
                    -- Create a keymap for vim.lsp.buf.implementation
                    vim.keymap.set('n', 'gd', vim.lsp.buf.definition, {})
                end
                if client.supports_method('textDocument/declaration') then
                    vim.keymap.set('n', 'gD', vim.lsp.buf.declaration, {})
                end
                if client.supports_method('textDocument/switchSourceHeader') then
                    vim.keymap.set('n', '<M-o>',
                        '<cmd>ClangdSwitchSourceHeader<cr>', {})
                end
            end,
        })

        -- local cmp_select = { behavior = cmp.SelectBehavior.Select }

        cmp.setup(
        {
            performance = {
                max_view_entries = 20
            },
            completion = {
                completeopt = "menu,menuone,noinsert"
            },
            -- preselect = cmp.PreselectMode.Item,
            mapping = cmp.mapping.preset.insert({
                -- ['<Tab>'] = cmp.mapping.select_prev_item(cmp_select),
                -- ['<S-Tab>'] = cmp.mapping.select_next_item(cmp_select),
                ['<Tab>'] = cmp.mapping.confirm({ select = true }),
                ['<Enter>'] = cmp.mapping.confirm({ select = true }),
            });
            sources = cmp.config.sources({
                { name = "nvim_lsp", },
                { name = "nvim_lsp_signature_help", },
            }),
        })

        -- vim.api.nvim_set_keymap('i', '<Esc>', [[pumvisible() ? "\<C-e><Esc>" : "\<Esc>"]],
        --     { expr = true, silent = true })
        -- vim.api.nvim_set_keymap('i', '<C-c>', [[pumvisible() ? "\<C-e><C-c>" : "\<C-c>"]],
        --     { expr = true, silent = true })
        -- vim.api.nvim_set_keymap('i', '<BS>', [[pumvisible() ? "\<C-e><BS>" : "\<BS>"]],
        --     { expr = true, silent = true })
        -- vim.api.nvim_set_keymap(
        --   "i",
        --   "<Tab>",
        --   [[pumvisible() ? (complete_info().selected == -1 ? "\<C-e><Tab>" : "\<C-y>") : "\<Tab>"]],
        --   { expr = true, silent = true }
        -- )
        -- I hate this
        -- vim.api.nvim_set_keymap('i', '.', '<cmd>lua vim.api.nvim_feedkeys(vim.api.nvim_replace_termcodes(".<C-x><C-u><C-e>", true, false, true), "n", true)<CR>', { noremap = true })
    end,
}
