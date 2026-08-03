-- Sample Neovim LSP configuration
vim.lsp.config('hlasm', {
    name = 'hlasm-language-server',
    cmd = { 'hlasm_language_server' }, -- path to the hlasm_language_server executable
    filetypes = { 'hlasm' },
    root_dir = vim.fs.dirname(vim.fs.find({'.hlasmplugin'}, {upward = true})[1]),
})
vim.lsp.enable('hlasm')

-- Define hlasm FileType to Neovim
vim.filetype.add({
    extension = {
        hlasm = 'hlasm'
    },
    pattern = {
        ['.*/[Aa][Ss][Mm][Pp][Gg][Mm]/[^/]*'] = 'hlasm',
        ['.*/[Aa][Ss][Mm][Mm][Aa][Cc]/[^/]*'] = 'hlasm',
        ['.*%.asm'] = 'hlasm',
        ['.*%.asmmac'] = 'hlasm',
        ['.*%.asmpgm'] = 'hlasm',
        ['.*%.hlasm'] = 'hlasm',
        ['.*'] = {
            priority = -math.huge,
            function(path, bufnr)
                local content = vim.api.nvim_buf_get_lines(bufnr, 0, 1000, false)
                for i = 1, #content do
                    m, e = string.match(content[i], '^[ ]+[Mm][Aa][Cc][Rr][Oo] ')
                    if m ~= nil then
                        return 'hlasm'
                    end
                    m, e = string.match(content[i], '^[ ]+[Mm][Aa][Cc][Rr][Oo]$')
                    if m ~= nil then
                        return 'hlasm'
                    end
                    c, e = string.match(content[i], '^[.]?*')
                    if c == nil then
                        break
                    end
                end
            end
        },
    },
})

-- Define hlasm virtual files buffer read command
local hlasm_group = vim.api.nvim_create_augroup("hlasm", {})
vim.api.nvim_create_autocmd('BufReadCmd', {
    group = hlasm_group,
    pattern = "hlasm://*",
    callback = function (args)
        local client = vim.lsp.get_clients({ name = "hlasm" })[1]
        if client == nil then
            return
        end

        local buf = args.buf

        vim.bo[buf].swapfile = false
        vim.bo[buf].buftype = 'nofile'

        local id = args.match:match("^%w+://([^/?#]+)")

        local resp = client:request_sync('get_virtual_file_content', { id = tonumber(id) })
        if resp == nil or resp.err ~= nil then
            return
        end

        local source_lines = vim.fn.split(resp.result.content, [[\r\?\n]])

        vim.api.nvim_buf_set_lines(buf, 0, -1, false, source_lines)
        vim.bo[buf].modifiable = false
    end,
})
