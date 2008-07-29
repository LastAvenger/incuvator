
(in-package :hurd-translator)

(def-fs-interface :dir-rmdir ((port port)
                              (name :string))
  (with-lookup protid port
    (block rmdir
           (let ((node (get-node protid))
                 (user (get-user protid)))
             (unless (is-dir-p (stat node))
               (return-from rmdir :not-directory))
             (let ((target (directory-lookup *translator* node user name)))
               (unless target
                 (return-from rmdir :no-such-file))
               (unless (zerop (- (number-of-entries *translator*
                                                    target
                                                    user)
                                 2)) ; Don't count . and ..
                 (return-from rmdir :directory-not-empty))
               (let ((err (remove-directory-entry *translator*
                                                  node
                                                  user
                                                  name
                                                  t)))
                 (cond
                   ((eq err t) t)
                   (t err))))))))
