Ditz::HookManager.on :after_add do |project, config, issues|
  issues.each do |issue|
    system 'git', 'add', issue.pathname
    system 'git', 'commit', '-m', "[bug-new] #{issue.title}"
  end
end

Ditz::HookManager.on :after_update do |project, config, issues|
  issues.each do |issue|
    system 'git', 'add', issue.pathname
    system 'git', 'commit', '-m', "[bug-#{issue.closed? ? "closed" : "changed"}] #{issue.title}"
  end
end

Ditz::HookManager.on :after_delete do |project, config, issues|
  issues.each do |issue|
    system 'git', 'rm', issue.pathname
    system 'git', 'commit', '-m', "[bug-remove] #{issue.title}"
  end
end

