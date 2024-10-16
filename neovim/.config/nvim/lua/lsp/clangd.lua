
function start_clangd(args)
    local proj_dir = vim.fs.root(args.buf, { "compile_commands.json" })
    if not proj_dir then
        proj_dir = vim.env.PWD
    end

    vim.lsp.start({
        name = "clangd",
        cmd = {"clangd"},
        root_dir = proj_dir
    })
end

vim.api.nvim_create_autocmd('FileType', {
    pattern = 'cpp',
    callback = start_clangd,
})

