require("lsp.clangd")

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
    -- holy moly
    if client.supports_method('textDocument/switchSourceHeader') then
        vim.keymap.set('n', '<M-o>',
            function()
                client.request('textDocument/switchSourceHeader', 
                    { uri = vim.uri_from_bufnr(args.buf) },
                    function(err, result)
                        if err then
                            error(tostring(err))
                        end
                        if not result then
                            print('Corresponding file cannot be determined')
                            return
                        end
                        vim.api.nvim_command('edit ' .. vim.uri_to_fname(result))
                    end, args.buf)
            end, {})
        print("clangd!")
    end
  end,
})

