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
